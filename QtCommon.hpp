/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef QTCOMMON_H
#define QTCOMMON_H

#include <QKeyEvent>
#include <QTimer>
#include <QtNetwork/QTcpSocket>
extern "C" {
#include "putty.h"
}

#define SUCCESS 0

#ifdef __linux
#define _snprintf snprintf
#define wsprintf(dst, fmt...) swprintf(dst, sizeof(dst) / sizeof(wchar_t), fmt)
#define _snwprintf swprintf
#else
#define snprintf _snprintf
#endif

class GuiMainWindow;
class GuiTerminalWindow;

extern QTimer *qtimer;
extern long timing_next_time;

typedef struct QtSocket QtSocket;

struct QtSocket {
  const char *error;
  QTcpSocket *qtsock;
  Plug *plug;
  void *private_ptr;
  bufchain output_data;
  bool connected;
  bool writable;
  bool frozen;          /* this causes readability notifications to be ignored */
  bool frozen_readable; /* this means we missed at least one readability
                         * notification while we were frozen */
  bool localhost_only;  /* for listening sockets */
  char oobdata[1];
  int sending_oob;
  bool oobinline, nodelay, keepalive, privport;
  SockAddr *addr;
  int port;
  bool pending_error; /* in case send() returns error */

  /*
   * We sometimes need pairs of Socket structures to be linked:
   * if we are listening on the same IPv6 and v4 port, for
   * example. So here we define `parent' and `child' pointers to
   * track this link.
   */
  QtSocket *parent, *child;

  Socket sock;
};

void qstring_to_char(char *dst, const QString &src, int dstlen);

class QTextCodec;
QTextCodec *getTextCodec(int line_codepage);
bool qtwin_is_utf8(TermWin *win);

class GuiTerminalWindow;
void qt_message_box(GuiTerminalWindow *frontend, const char *title, const char *fmt, ...);

#endif  // QTCOMMON_H
