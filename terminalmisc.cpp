/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include <QTextCodec>

#include "GuiTerminalWindow.hpp"
#include "QtCommon.hpp"
#include "terminal/terminal.h"

#if 0
void get_clip(void *frontend, wchar_t **p, int *len) {
  GuiTerminalWindow *f = static_cast<GuiTerminalWindow *>(frontend);
  f->getClip(p, len);
}
#endif

// int mk_wcwidth(wchar_t ucs){qDebug()<<"NOT_IMPL"<<__FUNCTION__;return 0;}
// int mk_wcwidth_cjk(wchar_t ucs){qDebug()<<"NOT_IMPL"<<__FUNCTION__;return 0;}

bool is_dbcs_leadbyte(int /*codepage*/, char /*byte*/) {
  qDebug() << "NOT_IMPL" << __FUNCTION__;
  return 0;
}

int mb_to_wc(int codepage, int /*flags*/, const char *mbstr, int mblen, wchar_t *wcstr,
             int /*wclen*/) {
  QTextCodec *codec = getTextCodec(codepage);
  if (!codec) return 0;
  return codec->toUnicode(mbstr, mblen).toWCharArray(wcstr);
}

int wc_to_mb(int codepage, int /*flags*/, const wchar_t *wcstr, int wclen, char *mbstr, int mblen,
             const char * /*defchr*/, struct unicode_data *ucsdata) {
  QTextCodec *codec = nullptr;
  if (ucsdata) getTextCodec(ucsdata->line_codepage);
  if (!codec) codec = getTextCodec(codepage);
  if (!codec) return 0;
  QByteArray mbarr = codec->fromUnicode(QString::fromWCharArray(wcstr, wclen));
  qstrncpy(mbstr, mbarr.constData(), mblen);
  return mbarr.length();
}

static bool qtwin_setup_draw_ctx(TermWin *win) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->preDrawTerm();
  return true;
}

static void qtwin_draw_text(TermWin *win, int x, int y, wchar_t *text, int len, unsigned long attrs,
                            int line_attrs, truecolour tc) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->drawText(y, x, text, len, attrs, line_attrs);
}

static void qtwin_draw_cursor(TermWin *, int x, int y, wchar_t *text, int len, unsigned long attrs,
                              int line_attrs, truecolour tc) {}

/* Draw the sigil indicating that a line of text has come from
 * PuTTY itself rather than the far end (defence against end-of-
 * authentication spoofing) */
static void qtwin_draw_trust_sigil(TermWin *, int x, int y) {}

static int qtwin_char_width(TermWin *, int uc) { return 1; }

static void qtwin_free_draw_ctx(TermWin *win) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->drawTerm();
}

static void qtwin_set_cursor_pos(TermWin *win, int x, int y) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  // ??
}

static void qtwin_set_raw_mouse_mode(TermWin *, bool enable) {}

static void qtwin_set_raw_mouse_mode_pointer(TermWin *, bool enable) {}

static void qtwin_set_scrollbar(TermWin *win, int total, int start, int page) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->setScrollBar(total, start, page);
}

static void qtwin_bell(TermWin *, int mode) {}

static void qtwin_clip_write(TermWin *win, int clipboard, wchar_t *text, int *attrs,
                             truecolour *colours, int len, bool must_deselect) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->writeClip(clipboard, text, attrs, colours, len, must_deselect);
}

static void qtwin_clip_request_paste(TermWin *win, int clipboard) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->requestPaste(clipboard);
}

static void qtwin_refresh(TermWin *win) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->repaint();
}

static void qtwin_request_resize(TermWin *, int w, int h) {}

static void qtwin_set_title(TermWin *win, const char *title, int codepage) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
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
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->setPalette(start, ncolours, colours);
}

void qtwin_palette_get_overrides(TermWin *, Terminal *) { qDebug() << __FUNCTION__; }

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
};

#if 0        
    qtwin_palette_reset,
    qtwin_get_pos,
    qtwin_get_pixels,
    qtwin_get_title,
    qtwin_is_utf8,
#endif
