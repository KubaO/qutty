/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include "GuiTerminalWindow.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>

#include "GuiFindToolBar.hpp"
#include "GuiMainWindow.hpp"
#include "GuiMenu.hpp"
#include "GuiSplitter.hpp"
#include "GuiTabWidget.hpp"
#include "QuTTY.hpp"
#include "serialize/QtWebPluginMap.hpp"

GuiTerminalWindow::GuiTerminalWindow(QWidget *parent, GuiMainWindow *mainWindow, PuttyConfig &&cfg)
    : QAbstractScrollArea(parent), cfgOwner(std::move(cfg)), cfg(cfgOwner.get()) {
  this->mainWindow = mainWindow;

  setFrameShape(QFrame::NoFrame);
  setWindowState(Qt::WindowMaximized);

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  connect(verticalScrollBar(), SIGNAL(actionTriggered(int)), this, SLOT(vertScrollBarAction(int)));
  connect(verticalScrollBar(), SIGNAL(sliderMoved(int)), this, SLOT(vertScrollBarMoved(int)));
  connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(vertScrollBarMoved(int)));

  setFocusPolicy(Qt::StrongFocus);

  mouseButtonAction = MA_NOTHING;
  setMouseTracking(true);
  viewport()->setCursor(Qt::IBeamCursor);
  Q_ASSERT(true);

  // enable drag-drop
  setAcceptDrops(true);
}

GuiTerminalWindow::~GuiTerminalWindow() {
  if (_tmuxMode == TMUX_MODE_GATEWAY && _tmuxGateway) {
    _tmuxGateway->initiateDetach();
    delete _tmuxGateway;
  }

  if (ldisc) {
    ldisc_free(ldisc);
    ldisc = NULL;
  }
  if (backend) {
    backend_free(backend);
    backend = NULL;
    term_provide_backend(term, NULL);
    term_free(term);
    if (qtsock) qtsock->close();
    term = NULL;
    qtsock = NULL;
  }
}

extern "C" Socket *get_ssh_socket(void *handle);
extern "C" Socket *get_telnet_socket(void *handle);

int GuiTerminalWindow::initTerminal() {
  char *realhost = NULL;
  char *ip_addr = conf_get_str(cfg, CONF_host);

  static_cast<TermWin *>(this)->vt = &qttermwin_vt;
  static_cast<Seat *>(this)->vt = &qtseat_vt;

  win_set_title(this, ip_addr, CP_ACP);

  memset(&ucsdata, 0, sizeof(struct unicode_data));
  init_ucs(cfg, &ucsdata);
  setTermFont(cfg);

  LogContext *logctx = log_init(default_logpolicy, cfg);

  const BackendVtable *vt = backend_vt_from_proto(conf_get_int(cfg, CONF_protocol));
  int port = conf_get_int(cfg, CONF_port);

  const char *error =
      backend_init(vt, this, &backend, logctx, cfg, (char *)ip_addr, port, &realhost, 1, 0);
  if (realhost) sfree(realhost);

  if (error) {
    qt_message_box(this, APPNAME " Error",
                   "Unable to open connection to\n"
                   "%.800s\n"
                   "%s",
                   conf_dest(cfg), error);
    backend = NULL;
    term = NULL;
    ldisc = NULL;
    goto cu0;
  }

  term = term_init(cfg, &ucsdata, this);
  logctx = log_init(default_logpolicy, cfg);
  term_provide_logctx(term, logctx);

  term_size(term, this->viewport()->height() / fontHeight, this->viewport()->width() / fontWidth,
            conf_get_int(cfg, CONF_savelines));

  switch (conf_get_int(cfg, CONF_protocol)) {
    case PROT_TELNET:
      as = get_telnet_socket(backend);
      break;
    case PROT_SSH:
      as = get_ssh_socket(backend);
      break;
    default:
      assert(0);
  }
  qtsock = sk_getqtsock(as);

  /*
   * Connect the terminal to the backend for resize purposes.
   */
  term_provide_backend(term, backend);

  /*
   * Set up a line discipline.
   */
  ldisc = ldisc_create(cfg, term, backend, this);

  return 0;

cu0:
  return -1;
}

static bool confUnequalInt(Conf *a, Conf *b, config_primary_key key) {
  return conf_get_int(a, key) != conf_get_int(b, key);
}

static bool confUnequalBool(Conf *a, Conf *b, config_primary_key key) {
  return conf_get_bool(a, key) != conf_get_bool(b, key);
}

static bool confUnequalStr(Conf *a, Conf *b, config_primary_key key) {
  return strcmp(conf_get_str(a, key), conf_get_str(b, key)) != 0;
}

static bool confUnequalFont(Conf *a, Conf *b, config_primary_key key) {
  FontSpec &a_font = *conf_get_fontspec(a, CONF_font);
  FontSpec &b_font = *conf_get_fontspec(b, CONF_font);
  return strcmp(a_font.name, b_font.name) != 0 || a_font.isbold != b_font.isbold ||
         a_font.height != b_font.height || a_font.charset != b_font.charset;
}

static bool confSizeChanged(Conf *a, Conf *b) {
  return confUnequalInt(a, b, CONF_height) || confUnequalInt(a, b, CONF_width) ||
         confUnequalInt(a, b, CONF_savelines) ||
         conf_get_int(a, CONF_resize_action) == RESIZE_FONT ||
         (conf_get_int(a, CONF_resize_action) == RESIZE_EITHER /*&& IsZoomed(hwnd)*/) ||
         conf_get_int(a, CONF_resize_action) == RESIZE_DISABLED;
}

char name[64];
int isbold;
int height;
int charset;

static bool confFontChanged(Conf *a, Conf *b) {
  int resize_action = conf_get_int(a, CONF_resize_action);
  return confUnequalFont(a, b, CONF_font) || confUnequalStr(a, b, CONF_line_codepage) ||
         confUnequalInt(a, b, CONF_font_quality) || confUnequalInt(a, b, CONF_vtmode) ||
#if 0
         confUnequalInt(a, b, CONF_bold_colour) ||
#endif
         resize_action == RESIZE_DISABLED || resize_action == RESIZE_EITHER ||
         confUnequalInt(a, b, CONF_resize_action);
}

int GuiTerminalWindow::restartTerminal() {
  if (_tmuxMode == TMUX_MODE_GATEWAY && _tmuxGateway) {
    _tmuxGateway->initiateDetach();
    delete _tmuxGateway;
  }

  if (ldisc) {
    ldisc_free(ldisc);
    ldisc = NULL;
  }
  if (backend) {
    backend_free(backend);
    backend = NULL;
    term_provide_backend(term, NULL);
    term_free(term);
    qtsock->close();
    term = NULL;
    qtsock = NULL;
  }
  isSockDisconnected = false;
  return initTerminal();
}

int GuiTerminalWindow::reconfigureTerminal(const PuttyConfig &new_cfg) {
  PuttyConfig prev_cfg = std::move(this->cfgOwner);
  this->cfgOwner = new_cfg.copy();
  this->cfg = cfgOwner.get();

  /* Pass new config data to the logging module */
  log_reconfig(term->logctx, cfg);

  /*
   * Flush the line discipline's edit buffer in the
   * case where local editing has just been disabled.
   */
  if (ldisc) ldisc_send(ldisc, NULL, 0, 0);

  /* Pass new config data to the terminal */
  term_reconfig(term, cfg);

  /* Pass new config data to the back end */
  if (backend) backend_reconfig(backend, cfg);

  /* Screen size changed ? */
  if (confSizeChanged(cfg, prev_cfg.get()))
    term_size(term, conf_get_int(cfg, CONF_height), conf_get_int(cfg, CONF_width),
              conf_get_int(cfg, CONF_savelines));

  if (confUnequalBool(cfg, prev_cfg.get(), CONF_alwaysontop)) {
    // TODO
  }

  if (confFontChanged(cfg, prev_cfg.get())) {
    init_ucs(cfg, &ucsdata);
    setTermFont(cfg);
  }

  repaint();
  return 0;
}

TmuxWindowPane *GuiTerminalWindow::initTmuxClientTerminal(TmuxGateway *gateway, int id, int width,
                                                          int height) {
  TmuxWindowPane *tmuxPane = NULL;

  memset(&ucsdata, 0, sizeof(struct unicode_data));
  init_ucs(cfg, &ucsdata);
  setTermFont(cfg);

  term = term_init(cfg, &ucsdata, this);
  LogContext *logctx = log_init(default_logpolicy, cfg);
  term_provide_logctx(term, logctx);
  int cfg_width = conf_get_int(cfg, CONF_width);
  int cfg_height = conf_get_int(cfg, CONF_height);
  // resize according to config if window is smaller
  if (!(mainWindow->windowState() & Qt::WindowMaximized) &&
      (mainWindow->size().width() < cfg_width * fontWidth ||
       mainWindow->size().height() < cfg_height * fontHeight)) {
    mainWindow->resize(cfg_width * fontWidth, cfg_height * fontHeight);
  }
  term_size(term, height, width, conf_get_int(cfg, CONF_savelines));

  _tmuxMode = TMUX_MODE_CLIENT;
  _tmuxGateway = gateway;
  conf_set_int(cfg, CONF_protocol, PROT_TMUX_CLIENT);
  conf_set_int(cfg, CONF_port, -1);
  conf_set_int(cfg, CONF_width, width);
  conf_set_int(cfg, CONF_height, height);

  const BackendVtable *vt = backend_vt_from_proto(conf_get_int(cfg, CONF_protocol));
  // HACK - pass paneid in port
  backend_init(vt, this, &backend, logctx, cfg, NULL, id, NULL, 0, 0);
  tmuxPane = new TmuxWindowPane(gateway, this);
  tmuxPane->id = id;
  tmuxPane->width = width;
  tmuxPane->height = height;

  as = NULL;
  qtsock = NULL;

  /*
   * Connect the terminal to the backend for resize purposes.
   */
  term_provide_backend(term, backend);

  /*
   * Set up a line discipline.
   */
  ldisc = ldisc_create(cfg, term, backend, this);
  return tmuxPane;
}

void GuiTerminalWindow::keyPressEvent(QKeyEvent *e) {
  noise_ultralight(NOISE_SOURCE_KEY, e->key());
  if (!term) return;

  // skip ALT SHIFT CTRL keypress events
  switch (e->key()) {
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Control:
    case Qt::Key_Shift:
      return;
  }

  char buf[16];
  int len = TranslateKey(e, buf);
  assert(len < 16);
  if (len > 0 || len == -2) {
    if (mainWindow->findToolBar) {
      mainWindow->findToolBar->findTextFlag = false;
      viewport()->repaint();
    }
    term_nopaste(term);
    term_seen_key_event(term);
    ldisc_send(ldisc, buf, len, 1);
    // show_mouseptr(0);
  } else if (len == -1) {
    wchar_t bufwchar[16];
    len = 0;
    // Treat Alt by just inserting an Esc before everything else
    if (!(e->modifiers() & Qt::ControlModifier) && e->modifiers() & Qt::AltModifier) {
      /*
       * In PuTTY, Left and right Alt will act differently
       * Ex: In english keybord layout, Left Alt + W = "" / Right Alt + W = "W"
       * In QuTTY, Left and left Alt will act same
       * Ex: In english keybord layout, Left Alt + W = "" / Right Alt + W = ""
       */
      bufwchar[len++] = 0x1b;
    }
    len += e->text().toWCharArray(bufwchar + len);
    if (len > 0 && len < 16) {
      if (mainWindow->findToolBar) {
        mainWindow->findToolBar->findTextFlag = false;
        viewport()->repaint();
      }
      term_nopaste(term);
      term_seen_key_event(term);
      term_keyinputw(term, bufwchar, len);
    }
  }
}

void GuiTerminalWindow::keyReleaseEvent(QKeyEvent *e) {
  noise_ultralight(NOISE_SOURCE_KEY, e->key());
}

void GuiTerminalWindow::highlightSearchedText() {
  /* The highlighting should trigger a repaint, and should be handled then and there.
   */
  if (mainWindow->getCurrentTerminal() != this) return;
  GuiFindToolBar *findToolBar = mainWindow->findToolBar;
  assert(findToolBar);

  const QString &text = findToolBar->currentSearchedText;
  unsigned long attr = 0;
  QString str = "";
  for (int y = 0; y < term->rows; y++) {
    str = QString::fromWCharArray(&term->dispstr[y * term->cols], term->cols);
    int index = 0;
    while ((index = str.indexOf(text, index, Qt::CaseInsensitive)) >= 0) {
      attr |= (8 << ATTR_FGSHIFT | (3 << ATTR_BGSHIFT));
      drawText(index, y, str.mid(index, text.length()), attr, 0, {});
      index = index + (text.length());
    }
  }
  str = QString::fromWCharArray(
      &term->dispstr[(findToolBar->currentRow - verticalScrollBar()->value()) * term->cols],
      term->cols);
  if (str.indexOf(text, findToolBar->currentCol, Qt::CaseInsensitive) >= 0) {
    int x = findToolBar->currentCol;
    int y = findToolBar->currentRow - verticalScrollBar()->value();
    drawText(x, y, findToolBar->currentSearchedText, ((5 << ATTR_FGSHIFT) | (7 << ATTR_BGSHIFT)), 0,
             {});
  }
}

void GuiTerminalWindow::paintEvent(QPaintEvent *e) {
  if (!term) return;
  qDebug() << __FUNCTION__;
  QPainter painter(viewport());
  this->painter = &painter;
  painter.fillRect(viewport()->rect(), colours[OSC4_COLOUR_bg]);
  term_paint(term, 0, 0, term->cols, term->rows, true);
  this->painter = nullptr;
}

bool GuiTerminalWindow::setupContext() {
  if (painter) {
    qDebug() << "painting";
    return true;
  }
  viewport()->update();
  qDebug() << "scheduled painting";
  return false;
}

QString decode(Terminal *term, const wchar_t *text, int len) {
  static QStringDecoder sd = QStringDecoder(QStringDecoder::System);
  QString str = QString(QStringView(text, len));
  QByteArray bytes;
  if (str.isEmpty()) return str;

  char16_t *start = nullptr;
  char16_t *end = nullptr;
  char16_t encoding = text[0] & CSET_MASK;

  switch (encoding) {
    case CSET_ASCII:
      for (int i = 0; i < len; i++) str[i] = QChar(text[i] & 0xFF);
      break;
    case CSET_OEMCP:
      for (int i = 0; i < len; i++) str[i] = QChar(term->ucsdata->unitab_oemcp[text[i] & 0xFF]);
      break;
    case CSET_LINEDRW:
      for (int i = 0; i < len; i++) str[i] = QChar(term->ucsdata->unitab_line[text[i] & 0xFF]);
      break;
    case CSET_SCOACS:
      for (int i = 0; i < len; i++) str[i] = QChar(term->ucsdata->unitab_scoacs[text[i] & 0xFF]);
      break;
    case CSET_ACP:
      bytes.resize(len);
      for (int i = 0; i < len; i++) bytes[i] = text[i] & 0xFF;
      str.resize(sd.requiredSpace(len));
      start = str.data_ptr().data();
      end = sd.appendToBuffer(start, bytes);
      str.resize(end - start);
      break;
  }
  return str;
}

void GuiTerminalWindow::drawText(int x, int y, const wchar_t *text, int len, unsigned long attrs,
                                 int lineAttrs, truecolour tc) {
  assert(painter);
  assert(!tc.fg.enabled && !tc.bg.enabled);
  drawText(x, y, decode(term, text, len), attrs, lineAttrs, tc);
}

void GuiTerminalWindow::drawText(int x, int y, const QString &text, unsigned long attrs,
                                 int lineAttrs, truecolour tc) {
  if ((attrs & TATTR_ACTCURS) && (conf_get_int(cfg, CONF_cursor_type) == 0 || term->big_cursor)) {
    attrs &= ~(ATTR_REVERSE | ATTR_BLINK | ATTR_COLOURS);
    if (bold_mode == BOLD_COLOURS) attrs &= ~ATTR_BOLD;

    /* cursor fg and bg */
    attrs |= (260 << ATTR_FGSHIFT) | (261 << ATTR_BGSHIFT);
  }

  int nfg = ((attrs & ATTR_FGMASK) >> ATTR_FGSHIFT);
  int nbg = ((attrs & ATTR_BGMASK) >> ATTR_BGSHIFT);
  if (attrs & ATTR_REVERSE) {
    std::swap(nfg, nbg);
  }
  if (bold_mode == BOLD_COLOURS && (attrs & ATTR_BOLD)) {
    if (nfg < 16)
      nfg |= 8;
    else if (nfg >= 256)
      nfg |= 1;
  }
  if (bold_mode == BOLD_COLOURS && (attrs & ATTR_BLINK)) {
    if (nbg < 16)
      nbg |= 8;
    else if (nbg >= 256)
      nbg |= 1;
  }

  QPen pen = QPen(colours[nfg]);
  QBrush brush = QBrush(colours[nbg]);
  drawText(x, y, text, pen, brush);
}

void GuiTerminalWindow::drawText(int x, int y, const QString &str, QPen pen, QBrush brush) {
  // qDebug() << __FUNCTION__ << x << y << str;
  painter->fillRect(QRect(x * fontWidth, y * fontHeight, fontWidth * str.length(), fontHeight),
                    brush);
  painter->setPen(pen);
  painter->drawText(x * fontWidth, y * fontHeight + fontAscent, str);
}

void GuiTerminalWindow::drawCursor(int x, int y, const wchar_t *text, int len, unsigned long attrs,
                                   int lineAttrs, truecolour tc) {
  int fnt_width;
  int char_width;
  int ctype = conf_get_int(cfg, CONF_cursor_type);

  QString str = decode(term, text, len);

  if ((attrs & TATTR_ACTCURS) && (ctype == 0 || term->big_cursor)) {
    return drawText(x, y, str, attrs, lineAttrs, tc);
  }

  fnt_width = char_width = fontWidth;
  if (attrs & ATTR_WIDE) char_width *= 2;
  x = x * fnt_width;
  y = x * fontHeight;

  if ((attrs & TATTR_PASCURS) && (ctype == 0 || term->big_cursor)) {
    QPoint points[] = {QPoint(x, y), QPoint(x, y + fontHeight - 1),
                       QPoint(x + char_width - 1, y + fontHeight - 1),
                       QPoint(x + char_width - 1, y)};
    painter->setPen(colours[OSC4_COLOUR_cursor_fg]);
    painter->drawPolygon(points, 4);
  } else if ((attrs & (TATTR_ACTCURS | TATTR_PASCURS)) && ctype != 0) {
    int startx, starty, dx, dy, length, i;
    if (ctype == 1) {
      startx = x;
      starty = y + fontMetrics().descent();
      dx = 1;
      dy = 0;
      length = char_width;
    } else {
      int xadjust = 0;
      if (attrs & TATTR_RIGHTCURS) xadjust = char_width - 1;
      startx = x + xadjust;
      starty = y;
      dx = 0;
      dy = 1;
      length = fontHeight;
    }
    if (attrs & TATTR_ACTCURS) {
      // To draw the vertical and underline active cursors
      painter->setPen(colours[OSC4_COLOUR_cursor_fg]);
      painter->drawLine(startx, starty + length * dx, startx + length * dx, starty + length);
    } else {
      // To draw the vertical and underline passive cursors
      painter->setPen(colours[OSC4_COLOUR_cursor_fg]);
      for (i = 0; i < length; i++) {
        if (i % 2 == 0) {
          painter->drawPoint(startx, starty + length * dx);
        }
        startx += dx;
        starty += dy;
      }
    }
  }
}

void GuiTerminalWindow::drawTrustSigil(int x, int y) {}

int GuiTerminalWindow::charWidth(int uc) { return 1; }

void GuiTerminalWindow::freeContext() { assert(painter); }

int GuiTerminalWindow::from_backend(SeatOutputType type, const char *data, size_t len) {
  if (_tmuxMode == TMUX_MODE_GATEWAY && _tmuxGateway) {
    size_t rc = _tmuxGateway->fromBackend(type == SEAT_OUTPUT_STDERR, data, len);
    if (rc && rc < len && _tmuxMode == TMUX_MODE_GATEWAY_DETACH_INIT) {
      detachTmuxControllerMode();
      return term_data(term, data + rc, (int)(len - rc));
    }
  }
  return term_data(term, data, (int)len);
}

void GuiTerminalWindow::setTermFont(Conf *cfg) {
  FontSpec &font = *conf_get_fontspec(cfg, CONF_font);
  int font_quality = conf_get_int(cfg, CONF_font_quality);
  _font.setFamily(font.name);
  _font.setPointSize(font.height);
  _font.setStyleHint(QFont::TypeWriter);

  if (font_quality == FQ_NONANTIALIASED)
    _font.setStyleStrategy(QFont::NoAntialias);
  else if (font_quality == FQ_ANTIALIASED)
    _font.setStyleStrategy(QFont::PreferAntialias);
  setFont(_font);

  QFontMetrics fontMetrics = QFontMetrics(_font);
  fontWidth = fontMetrics.horizontalAdvance(QChar('a'));
  fontHeight = fontMetrics.height();
  fontAscent = fontMetrics.ascent();
}

void GuiTerminalWindow::setPalette(unsigned start, unsigned ncolours, const rgb *colours) {
  for (int i = start; i < start + ncolours; ++i) {
    const rgb &c = colours[i];
    this->colours[i] = QColor::fromRgb(c.r, c.g, c.b);
  }

  /* Override with system colours if appropriate * /
  if (conf_get_int(cfg, CONF_system_colour))
      systopalette();*/
}

/*
 * Translate a raw mouse button designation (LEFT, MIDDLE, RIGHT)
 * into a cooked one (SELECT, EXTEND, PASTE).
 */
static Mouse_Button translate_button(Conf *cfg, Mouse_Button button) {
  if (button == MBT_LEFT) return MBT_SELECT;
  int mouse_is_xterm = conf_get_int(cfg, CONF_mouse_is_xterm);
  if (button == MBT_MIDDLE) return mouse_is_xterm == 1 ? MBT_PASTE : MBT_EXTEND;
  if (button == MBT_RIGHT) return mouse_is_xterm == 1 ? MBT_EXTEND : MBT_PASTE;
  assert(0);
  return MBT_NOTHING; /* shouldn't happen */
}

void GuiTerminalWindow::mouseDoubleClickEvent(QMouseEvent *e) {
  QPoint pos = e->position().toPoint();
  noise_ultralight(NOISE_SOURCE_MOUSEPOS, pos.x() << 16 | pos.y());
  if (!term) return;

  int mouse_is_xterm = conf_get_int(cfg, CONF_mouse_is_xterm);
  if (e->button() == Qt::RightButton &&
      ((e->modifiers() & Qt::ControlModifier) || (mouse_is_xterm == 2))) {
    // TODO right click menu
  }
  Mouse_Button button, bcooked;
  button = e->button() == Qt::LeftButton     ? MBT_LEFT
           : e->button() == Qt::RightButton  ? MBT_RIGHT
           : e->button() == Qt::MiddleButton ? MBT_MIDDLE
                                             : MBT_NOTHING;
  // assert(button!=MBT_NOTHING);
  if (button == MBT_NOTHING) return;
  int x = pos.x() / fontWidth, y = pos.y() / fontHeight, mod = e->modifiers();
  bcooked = translate_button(cfg, button);

  // detect single/double/triple click
  mouseClickTimer.start();
  mouseButtonAction = MA_2CLK;

  qDebug() << __FUNCTION__ << x << y << mod << button << bcooked << mouseButtonAction;
  term_mouse(term, button, bcooked, mouseButtonAction, x, y, mod & Qt::ShiftModifier,
             mod & Qt::ControlModifier, mod & Qt::AltModifier);
  e->accept();
}

//#define (e) e->button()&Qt::LeftButton
void GuiTerminalWindow::mouseMoveEvent(QMouseEvent *e) {
  QPoint pos = e->position().toPoint();
  noise_ultralight(NOISE_SOURCE_MOUSEPOS, pos.x() << 16 | pos.y());
  if (e->buttons() == Qt::NoButton) {
    mainWindow->toolBarTerminalTop.processMouseMoveTerminalTop(this, e);
    return;
  }
  if (!term) return;

  int mouse_is_xterm = conf_get_int(cfg, CONF_mouse_is_xterm);
  if (e->buttons() == Qt::LeftButton &&
      ((e->modifiers() & Qt::ControlModifier) || (mouse_is_xterm == 2)) &&
      (e->pos() - dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
    // start of drag
    this->dragStartEvent(e);
    return;
  }

  Mouse_Button button, bcooked;
  button = e->buttons() & Qt::LeftButton     ? MBT_LEFT
           : e->buttons() & Qt::RightButton  ? MBT_RIGHT
           : e->buttons() & Qt::MiddleButton ? MBT_MIDDLE
                                             : MBT_NOTHING;
  // assert(button!=MBT_NOTHING);
  if (button == MBT_NOTHING) return;
  int x = pos.x() / fontWidth, y = pos.y() / fontHeight, mod = e->modifiers();
  bcooked = translate_button(cfg, button);
  term_mouse(term, button, bcooked, MA_DRAG, x, y, mod & Qt::ShiftModifier,
             mod & Qt::ControlModifier, mod & Qt::AltModifier);
  e->accept();
}

// Qt 5.0 supports qApp->styleHints()->mouseDoubleClickInterval()
#define CFG_MOUSE_TRIPLE_CLICK_INTERVAL 500

void GuiTerminalWindow::mousePressEvent(QMouseEvent *e) {
  QPoint pos = e->position().toPoint();
  noise_ultralight(NOISE_SOURCE_MOUSEPOS, pos.x() << 16 | pos.y());
  if (!term) return;

  int mouse_is_xterm = conf_get_int(cfg, CONF_mouse_is_xterm);
  if (e->button() == Qt::LeftButton &&
      ((e->modifiers() & Qt::ControlModifier) || (mouse_is_xterm == 2))) {
    // possible start of drag
    dragStartPos = e->pos();
  }

  if (e->button() == Qt::RightButton &&
      ((e->modifiers() & Qt::ControlModifier) || (mouse_is_xterm == 2))) {
    // right click menu
    this->showContextMenu(e);
    e->accept();
    return;
  }
  Mouse_Button button, bcooked;
  button = e->button() == Qt::LeftButton     ? MBT_LEFT
           : e->button() == Qt::RightButton  ? MBT_RIGHT
           : e->button() == Qt::MiddleButton ? MBT_MIDDLE
                                             : MBT_NOTHING;
  // assert(button!=MBT_NOTHING);
  if (button == MBT_NOTHING) return;
  int x = pos.x() / fontWidth, y = pos.y() / fontHeight, mod = e->modifiers();
  bcooked = translate_button(cfg, button);

  // detect single/double/triple click
  if (button == MBT_LEFT && !mouseClickTimer.hasExpired(CFG_MOUSE_TRIPLE_CLICK_INTERVAL)) {
    mouseButtonAction = mouseButtonAction == MA_CLICK  ? MA_2CLK
                        : mouseButtonAction == MA_2CLK ? MA_3CLK
                        : mouseButtonAction == MA_3CLK ? MA_CLICK
                                                       : MA_NOTHING;
    qDebug() << __FUNCTION__ << "not expired" << mouseButtonAction;
  } else
    mouseButtonAction = MA_CLICK;

  term_mouse(term, button, bcooked, mouseButtonAction, x, y, mod & Qt::ShiftModifier,
             mod & Qt::ControlModifier, mod & Qt::AltModifier);
  e->accept();
}

void GuiTerminalWindow::mouseReleaseEvent(QMouseEvent *e) {
  QPoint pos = e->position().toPoint();
  noise_ultralight(NOISE_SOURCE_MOUSEPOS, pos.x() << 16 | pos.y());
  noise_ultralight(NOISE_SOURCE_MOUSEBUTTON, e->buttons());
  if (!term) return;

  Mouse_Button button, bcooked;
  button = e->button() == Qt::LeftButton     ? MBT_LEFT
           : e->button() == Qt::RightButton  ? MBT_RIGHT
           : e->button() == Qt::MiddleButton ? MBT_MIDDLE
                                             : MBT_NOTHING;
  // assert(button!=MBT_NOTHING);
  if (button == MBT_NOTHING) return;
  int x = pos.x() / fontWidth, y = pos.y() / fontHeight, mod = e->modifiers();
  bcooked = translate_button(cfg, button);
  term_mouse(term, button, bcooked, MA_RELEASE, x, y, mod & Qt::ShiftModifier,
             mod & Qt::ControlModifier, mod & Qt::AltModifier);
  e->accept();
}

void GuiTerminalWindow::getClip(wchar_t **p, int *len) {
  if (p && len) {
    if (clipboard_contents) delete clipboard_contents;
    QString s = QApplication::clipboard()->text();
    clipboard_length = s.length() + 1;
    clipboard_contents = new wchar_t[clipboard_length];
    clipboard_length = s.toWCharArray(clipboard_contents);
    clipboard_contents[clipboard_length] = 0;
    *p = clipboard_contents;
    *len = clipboard_length;
  }
}

void GuiTerminalWindow::requestPaste(int clipboard) {
  auto text = QApplication::clipboard()->text().toStdWString();
  term_do_paste(term, text.c_str(), text.size());

  // Save the paste in our persistent list.
  qutty_web_plugin_map.hash_map["PASTE_HISTORY"].prepend(QApplication::clipboard()->text());
  qutty_web_plugin_map.save();
}

void GuiTerminalWindow::writeClip(int /*clipboard*/, wchar_t *data, int * /*attr*/,
                                  truecolour * /*colours*/, int len, int /*must_deselect*/) {
  data[len] = 0;
  QString s = QString::fromWCharArray(data);
  QApplication::clipboard()->setText(s);
}

void GuiTerminalWindow::resizeEvent(QResizeEvent *) {
  if (viewport()->height() == 0 || viewport()->width() == 0) {
    // skip the spurious resizes during split-pane create/delete/drag-drop
    // we are not missing anything by not resizing, since height/width is 0
    return;
  }

  if (_tmuxMode == TMUX_MODE_CLIENT) {
    int width = viewport()->size().width() / fontWidth;
    int height = viewport()->size().height() / fontHeight;
    if (parentSplit) {
      width = parentSplit->width() / fontWidth;
      height = parentSplit->height() / fontHeight;
    }
    wchar_t cmd_resize[128];
    wsprintf(cmd_resize, L"refresh-client -C %d,%d\n", width, height);
    _tmuxGateway->sendCommand(_tmuxGateway, CB_NULL, cmd_resize);
    // %layout-change tmux command does the actual resize
    return;
  }
  if (term)
    term_size(term, viewport()->size().height() / fontHeight,
              viewport()->size().width() / fontWidth, conf_get_int(cfg, CONF_savelines));
}

bool GuiTerminalWindow::event(QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Tab) {
      // ctrl + tab
      keyPressEvent(keyEvent);
      qDebug() << "key" << keyEvent;
      return true;
    } else if (keyEvent->key() == Qt::Key_Backtab) {
      // ctrl + shift + tab
      keyPressEvent(keyEvent);
      qDebug() << "key" << keyEvent;
      return true;
    }
  }
  return QAbstractScrollArea::event(event);
}

void GuiTerminalWindow::focusInEvent(QFocusEvent *) {
  this->mru_count = ++mainWindow->mru_count_last;
  if (!term) return;
  term_set_focus(term, TRUE);
  viewport()->update();
  if (parentSplit) mainWindow->tabArea->setTabText(mainWindow->tabArea->currentIndex(), temp_title);
}

void GuiTerminalWindow::focusOutEvent(QFocusEvent *) {
  if (!term) return;
  term_set_focus(term, FALSE);
  viewport()->update();
}

void GuiTerminalWindow::setScrollBar(int total, int start, int page) {
  verticalScrollBar()->setPageStep(page);
  verticalScrollBar()->setRange(0, total - page);
  if (verticalScrollBar()->value() != start) verticalScrollBar()->setValue(start);
}

void GuiTerminalWindow::vertScrollBarAction(int action) {
  if (!term) return;
  switch (action) {
    case QAbstractSlider::SliderSingleStepAdd:
      term_scroll(term, 0, +1);
      break;
    case QAbstractSlider::SliderSingleStepSub:
      term_scroll(term, 0, -1);
      break;
    case QAbstractSlider::SliderPageStepAdd:
      term_scroll(term, 0, +term->rows / 2);
      break;
    case QAbstractSlider::SliderPageStepSub:
      term_scroll(term, 0, -term->rows / 2);
      break;
  }
}

void GuiTerminalWindow::vertScrollBarMoved(int value) {
  if (!term) return;
  term_scroll(term, 1, value);
}

int GuiTerminalWindow::initTmuxControllerMode(char * /*tmux_version*/) {
  // TODO version check
  assert(_tmuxMode == TMUX_MODE_NONE);

  qDebug() << "TMUX mode entered";
  _tmuxMode = TMUX_MODE_GATEWAY;
  _tmuxGateway = new TmuxGateway(this);

  mainWindow->hideTerminal(this);
  return 0;
}

void GuiTerminalWindow::startDetachTmuxControllerMode() {
  _tmuxMode = TMUX_MODE_GATEWAY_DETACH_INIT;
}

void GuiTerminalWindow::detachTmuxControllerMode() {
  assert(_tmuxMode == TMUX_MODE_GATEWAY_DETACH_INIT);

  _tmuxGateway->detach();
  delete _tmuxGateway;
  _tmuxGateway = NULL;
  _tmuxMode = TMUX_MODE_NONE;
}

void GuiTerminalWindow::closeTerminal() {
  // be sure to hide the top-right menu
  this->getMainWindow()->toolBarTerminalTop.hideMe();
  if (parentSplit) {
    parentSplit->removeSplitLayout(this);
  }
  mainWindow->closeTerminal(this);
  this->close();
  this->deleteLater();
}

void GuiTerminalWindow::reqCloseTerminal(bool userConfirm) {
  userClosingTab = true;
  if (!userConfirm && conf_get_bool(cfg, CONF_warn_on_close) && !isSockDisconnected &&
      QMessageBox::No == QMessageBox::question(this, "Exit Confirmation?",
                                               "Are you sure you want to close this session?",
                                               QMessageBox::Yes | QMessageBox::No))
    return;
  this->closeTerminal();
}

void GuiTerminalWindow::on_sessionTitleChange(bool force) {
  int tabind = mainWindow->getTerminalTabInd(this);
  QString title = "";
  title += QString::number(tabind + 1) + ". ";
  if (!custom_title.isEmpty() && !runtime_title.isEmpty())
    title += custom_title + " - " + runtime_title;
  else if (!custom_title.isEmpty())
    title += custom_title;
  else if (!runtime_title.isEmpty())
    title += runtime_title;
  if (title == temp_title && !force) return;
  temp_title = title;
  if (!parentSplit || mainWindow->tabArea->widget(tabind)->focusWidget() == this)
    mainWindow->tabArea->setTabText(tabind, temp_title);
}

#if 0
void get_clip(void *frontend, wchar_t **p, int *len) {
  GuiTerminalWindow *f = static_cast<GuiTerminalWindow *>(frontend);
  f->getClip(p, len);
}
#endif

static bool qtwin_setup_draw_ctx(TermWin *win) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  return gw->setupContext();
}

static void qtwin_draw_text(TermWin *win, int x, int y, wchar_t *text, int len, unsigned long attrs,
                            int line_attrs, truecolour tc) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  gw->drawText(x, y, text, len, attrs, line_attrs, tc);
}

static void qtwin_draw_cursor(TermWin *win, int x, int y, wchar_t *text, int len,
                              unsigned long attrs, int line_attrs, truecolour tc) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  gw->drawCursor(x, y, text, len, attrs, line_attrs, tc);
}

/* Draw the sigil indicating that a line of text has come from
 * PuTTY itself rather than the far end (defence against end-of-
 * authentication spoofing) */
static void qtwin_draw_trust_sigil(TermWin *win, int x, int y) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  gw->drawTrustSigil(x, y);
}

static int qtwin_char_width(TermWin *win, int uc) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  return gw->charWidth(uc);
}

static void qtwin_free_draw_ctx(TermWin *win) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  gw->freeContext();
}

static void qtwin_set_cursor_pos(TermWin *win, int x, int y) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  // ??
}

static void qtwin_set_raw_mouse_mode(TermWin *, bool enable) {}

static void qtwin_set_raw_mouse_mode_pointer(TermWin *, bool enable) {}

static void qtwin_set_scrollbar(TermWin *win, int total, int start, int page) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  gw->setScrollBar(total, start, page);
}

static void qtwin_bell(TermWin *, int mode) {}

static void qtwin_clip_write(TermWin *win, int clipboard, wchar_t *text, int *attrs,
                             truecolour *colours, int len, bool must_deselect) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  gw->writeClip(clipboard, text, attrs, colours, len, must_deselect);
}

static void qtwin_clip_request_paste(TermWin *win, int clipboard) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  gw->requestPaste(clipboard);
}

static void qtwin_refresh(TermWin *win) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  gw->repaint();
}

static void qtwin_request_resize(TermWin *, int w, int h) {}

static void qtwin_set_title(TermWin *win, const char *title, int codepage) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  gw->setSessionTitle(QString::fromLatin1(title));  // FIXME TODO wrong encoding
}

void qtwin_set_icon_title(TermWin *, const char *icontitle, int codepage) {}

/* set_minimised and set_maximised are assumed to set two
 * independent settings, rather than a single three-way
 * {min,normal,max} switch. The idea is that when you un-minimise
 * the window it remembers whether to go back to normal or
 * maximised. */

void qtwin_set_minimised(TermWin *, bool minimised) {}

void qtwin_set_maximised(TermWin *, bool maximised) {}

//

void qtwin_move(TermWin *, int x, int y) {}
void qtwin_set_zorder(TermWin *, bool top) {}

void qtwin_palette_set(TermWin *win, unsigned start, unsigned ncolours, const rgb *colours) {
  qDebug() << __FUNCTION__ << start << ncolours;
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  gw->setPalette(start, ncolours, colours);
}

void qtwin_palette_get_overrides(TermWin *, Terminal *) { qDebug() << __FUNCTION__; }

void qtwin_unthrottle(TermWin *win, size_t bufsize) {
  qDebug() << __FUNCTION__ << bufsize;
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  backend_unthrottle(gw->backend, bufsize);
}

const TermWinVtable qttermwin_vt = {
    qtwin_setup_draw_ctx,
    qtwin_draw_text,
    qtwin_draw_cursor,
    qtwin_draw_trust_sigil,
    qtwin_char_width,
    qtwin_free_draw_ctx,
    qtwin_set_cursor_pos,
    qtwin_set_raw_mouse_mode,
    qtwin_set_raw_mouse_mode_pointer,
    qtwin_set_scrollbar,
    qtwin_bell,
    qtwin_clip_write,
    qtwin_clip_request_paste,
    qtwin_refresh,
    qtwin_request_resize,
    qtwin_set_title,
    qtwin_set_icon_title,
    //
    qtwin_set_minimised,
    qtwin_set_maximised,
    qtwin_move,
    qtwin_set_zorder,
    qtwin_palette_set,
    qtwin_palette_get_overrides,
    qtwin_unthrottle,
};
