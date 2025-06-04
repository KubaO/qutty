/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef TERMINALWINDOW_H
#define TERMINALWINDOW_H

#include <QAbstractScrollArea>
#include <QAbstractSocket>
#include <QElapsedTimer>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QPointer>

#include "GuiBase.hpp"
#include "GuiDrag.hpp"
#include "QtCommon.hpp"
#include "QtConfig.hpp"
#include "tmux/TmuxGateway.hpp"
#include "tmux/TmuxWindowPane.hpp"
#include "tmux/tmux.h"
extern "C" {
#include "putty.h"
#include "terminal/terminal.h"
}

class GuiMainWindow;

class GuiTerminalWindow : public QAbstractScrollArea, public GuiBase, public TermWin, public Seat {
  Q_OBJECT
  Q_INTERFACES(GuiBase)

 private:
  enum tmux_mode_t _tmuxMode = TMUX_MODE_NONE;
  TmuxGateway *_tmuxGateway = nullptr;
  GuiMainWindow *mainWindow = nullptr;

  void showContextMenu(QMouseEvent *e);

  QFont _font;
  int fontWidth, fontHeight, fontAscent;
  struct unicode_data ucsdata = {};
  Socket *as = nullptr;
  QPointer<QAbstractSocket> qtsock;
  bool _any_update = false;
  QRegion termrgn;
  std::array<QColor, OSC4_NCOLOURS> colours;

  /* Painter that's active during window repaint */
  QPainter *painter = nullptr;

  // to detect mouse double/triple clicks
  Mouse_Action mouseButtonAction;
  QElapsedTimer mouseClickTimer;

  enum { BOLD_COLOURS, BOLD_SHADOW, BOLD_FONT } bold_mode;

  // members for drag-drop support
  QPoint dragStartPos;

  // support for clipboard paste
  wchar_t *clipboard_contents = nullptr;
  int clipboard_length = 0;

  // session title
  QString runtime_title;  // given by terminal/shell
  QString custom_title;   // given by user
  QString temp_title;

  PuttyConfig cfgOwner;
  Conf *cfg;

 public:
  Terminal *term = nullptr;
  Backend *backend = nullptr;
  Ldisc *ldisc = nullptr;

  bool userClosingTab = false;
  bool isSockDisconnected = false;

  // order-of-usage
  uint32_t mru_count = 0;

  const PuttyConfig &config() const { return cfgOwner; }
  Conf *getCfg() const { return cfgOwner.get(); }

  GuiTerminalWindow(QWidget *parent, GuiMainWindow *mainWindow, PuttyConfig &&cfg);
  ~GuiTerminalWindow() override;

  GuiMainWindow *getMainWindow() { return mainWindow; }

  /*
   * follow a two phased construction
   * 1. GuiMainWindow::newTerminal()  -> create
   * 2. termWnd->initTerminal() -> init the internals with config
   */
  int initTerminal();
  int restartTerminal();
  int reconfigureTerminal(const PuttyConfig &new_cfg);

  void createSplitLayout(GuiBase::SplitType split, GuiTerminalWindow *newTerm);

  void keyPressEvent(QKeyEvent *e) override;
  void keyReleaseEvent(QKeyEvent *e) override;
  int from_backend(SeatOutputType type, const char *data, size_t len);

  bool setupContext();
  void drawText(int x, int y, const wchar_t *text, int len, unsigned long attrs, int lineAttrs,
                truecolour tc);
  void drawText(int x, int y, const QString &str, unsigned long attrs, int lineAttrs,
                truecolour tc);
  void drawText(int x, int y, const QString &str, QPen pen, QBrush brush);
  void drawCursor(int x, int y, const wchar_t *text, int len, unsigned long attrs, int lineAttrs,
                  truecolour tc);
  void drawTrustSigil(int x, int y);
  int charWidth(int uc);
  void freeContext();

  void setTermFont(Conf *cfg);
  void setPalette(unsigned start, unsigned ncolours, const rgb *colours);
  void requestPaste(int clipboard);
  void getClip(wchar_t **p, int *len);

  void writeClip(int clipboard, wchar_t *data, int *attr, truecolour *colours, int len,
                 int must_deselect);

  void setScrollBar(int total, int start, int page);
  int TranslateKey(QKeyEvent *keyevent, char *output);

  int initTmuxControllerMode(char *tmux_version);
  TmuxWindowPane *initTmuxClientTerminal(TmuxGateway *gateway, int id, int width, int height);
  void startDetachTmuxControllerMode();
  TmuxGateway *tmuxGateway() { return _tmuxGateway; }

  void closeTerminal();
  void reqCloseTerminal(bool userConfirm) override;
  void highlightSearchedText();
  int getFontWidth() { return fontWidth; };
  int getFontHeight() { return fontHeight; };

  QWidget *getWidget() override { return this; }

  // Needed functions for drag-drop support
  void dragStartEvent(QMouseEvent *e);
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dragLeaveEvent(QDragLeaveEvent *e) override;
  void dragMoveEvent(QDragMoveEvent *e) override;
  void dropEvent(QDropEvent *e) override;

  void populateAllTerminals(std::vector<GuiTerminalWindow *> &list) override {
    list.push_back(this);
  }

  const QString &getSessionTitle() const { return runtime_title; }
  const QString &getCustomSessionTitle() const { return custom_title; }
  void setSessionTitle(const QString &t) {
    if (runtime_title != t) {
      runtime_title = t;
      on_sessionTitleChange();
    }
  }
  void setCustomSessionTitle(const QString &t) {
    custom_title = t;
    on_sessionTitleChange();
  }

 protected:
  void paintEvent(QPaintEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;
  bool event(QEvent *event) override;
  void focusInEvent(QFocusEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;

 public slots:
  void vertScrollBarAction(int action);
  void vertScrollBarMoved(int value);
  void detachTmuxControllerMode();
  void on_sessionTitleChange(bool force = false);
};

extern const TermWinVtable qttermwin_vt;
extern const SeatVtable qtseat_vt;

#endif  // TERMINALWINDOW_H
