/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef QTCOMMON_H
#define QTCOMMON_H

#include <QKeyEvent>

extern "C" {
#include "defs.h"
}

#ifdef __linux
#define _snprintf snprintf
#define wsprintf(dst, fmt...) swprintf(dst, sizeof(dst) / sizeof(wchar_t), fmt)
#define _snwprintf swprintf
#else
#define snprintf _snprintf
#endif

class GuiMainWindow;
class GuiTerminalWindow;
class QAbstractSocket;
class QTextCodec;
class QTimer;

extern QTimer *qtimer;
extern long timing_next_time;

QAbstractSocket *sk_getqtsock(Socket *socket);

void qstring_to_char(char *dst, const QString &src, int dstlen);

QTextCodec *getTextCodec(int line_codepage);
bool qtwin_is_utf8(TermWin *win);

QString filename_to_qstring(const Filename *fn);
Filename *filename_from_qstring(const QString &str);

void qt_message_box(GuiTerminalWindow *frontend, const char *title, const char *fmt, ...);

extern LogPolicy default_logpolicy[1];

#endif  // QTCOMMON_H
