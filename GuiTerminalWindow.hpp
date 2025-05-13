/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef TERMINALWINDOW_H
#define TERMINALWINDOW_H

#include <QAbstractScrollArea>
#include <QElapsedTimer>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QtNetwork/QTcpSocket>

#include "GuiBase.hpp"
#include "GuiDrag.hpp"
#include "QtCommon.hpp"
#include "tmux/TmuxGateway.hpp"
#include "tmux/TmuxWindowPane.hpp"
#include "tmux/tmux.h"
extern "C" {
#include "putty.h"
}

#define NCFGCOLOURS 22
#define NEXTCOLOURS 240
#define NALLCOLOURS (NCFGCOLOURS + NEXTCOLOURS)

class GuiMainWindow;

class GuiTerminalWindow : public QAbstractScrollArea, public GuiBase {
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
  Actual_Socket as = nullptr;
  QTcpSocket *qtsock = nullptr;
  bool _any_update = false;
  QRegion termrgn;
  QColor colours[NALLCOLOURS];

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

 public:
  Config cfg = {};
  Terminal *term = nullptr;
  Backend *backend = nullptr;
  void *backhandle = nullptr;
  void *ldisc = nullptr;

  bool userClosingTab = false;
  bool isSockDisconnected = false;

  // order-of-usage
  uint32_t mru_count = 0;

  explicit GuiTerminalWindow(QWidget *parent, GuiMainWindow *mainWindow);
  ~GuiTerminalWindow() override;

  GuiMainWindow *getMainWindow() { return mainWindow; }

  /*
   * follow a two phased construction
   * 1. GuiMainWindow::newTerminal()  -> create
   * 2. termWnd->initTerminal() -> init the internals with config
   */
  int initTerminal();
  int restartTerminal();
  int reconfigureTerminal(Config *new_cfg);

  void createSplitLayout(GuiBase::SplitType split, GuiTerminalWindow *newTerm);

  void keyPressEvent(QKeyEvent *e) override;
  void keyReleaseEvent(QKeyEvent *e) override;
  int from_backend(int is_stderr, const char *data, size_t len);
  void preDrawTerm();
  void drawTerm();
  void drawText(int row, int col, wchar_t *ch, int len, unsigned long attr, int lattr);

  void setTermFont(Config *cfg);
  void cfgtopalette(Config *cfg);
  void requestPaste();
  void getClip(wchar_t **p, int *len);
  void writeClip(wchar_t *data, int *attr, int len, int must_deselect);
  void paintText(QPainter &painter, int row, int col, const QString &str, unsigned long attr);
  void paintCursor(QPainter &painter, int row, int col, const QString &str, unsigned long attr);

  void setScrollBar(int total, int start, int page);
  int TranslateKey(QKeyEvent *keyevent, char *output);

  int initTmuxControllerMode(char *tmux_version);
  TmuxWindowPane *initTmuxClientTerminal(TmuxGateway *gateway, int id, int width, int height);
  void startDetachTmuxControllerMode();
  TmuxGateway *tmuxGateway() { return _tmuxGateway; }

  void closeTerminal();
  void reqCloseTerminal(bool userConfirm) override;
  void highlightSearchedText(QPainter &painter);
  int getFontWidth() { return fontWidth; };
  int getFontHeight() { return fontHeight; };

  QWidget *getWidget() override { return this; }

  // Needed functions for drag-drop support
  void dragStartEvent(QMouseEvent *e);
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dragLeaveEvent(QDragLeaveEvent *e) override;
  void dragMoveEvent(QDragMoveEvent *e) override;
  void dropEvent(QDropEvent *e) override;

  void populateAllTerminals(std::vector<GuiTerminalWindow *> *list) override {
    list->push_back(this);
  }

  QString getSessionTitle() { return runtime_title; }
  QString getCustomSessionTitle() { return custom_title; }
  void setSessionTitle(QString t) {
    if (runtime_title != t) {
      runtime_title = t;
      on_sessionTitleChange();
    }
  }
  void setCustomSessionTitle(QString t) {
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
  void readyRead();
  void vertScrollBarAction(int action);
  void vertScrollBarMoved(int value);
  void detachTmuxControllerMode();
  void sockError(QAbstractSocket::SocketError socketError);
  void sockDisconnected();
  void on_sessionTitleChange(bool force = false);
};

#endif  // TERMINALWINDOW_H
