/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include <QTextCodec>

#include "GuiTerminalWindow.hpp"
#include "QtCommon.hpp"
#include "terminal.h"

/* Dummy routine, only required in plink. */
void ldisc_update(void * /*frontend*/, int /*echo*/, int /*edit*/) {}

void frontend_keypress(void * /*handle*/) {
  /*
   * Keypress termination in non-Close-On-Exit mode is not
   * currently supported in PuTTY proper, because the window
   * always has a perfectly good Close button anyway. So we do
   * nothing here.
   */
  return;
}

printer_job *printer_start_job(char * /*printer*/) { return NULL; }
void printer_job_data(printer_job * /*pj*/, void * /*data*/, int /*len*/) {}
void printer_finish_job(printer_job * /*pj*/) {}

#if 0
void get_clip(void *frontend, wchar_t **p, int *len) {
  GuiTerminalWindow *f = static_cast<GuiTerminalWindow *>(frontend);
  f->getClip(p, len);
}
#endif

// int mk_wcwidth(wchar_t ucs){qDebug()<<"NOT_IMPL"<<__FUNCTION__;return 0;}
// int mk_wcwidth_cjk(wchar_t ucs){qDebug()<<"NOT_IMPL"<<__FUNCTION__;return 0;}

int is_dbcs_leadbyte(int /*codepage*/, char /*byte*/) {
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
             const char * /*defchr*/, int * /*defused*/, struct unicode_data *ucsdata) {
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
                            int line_attrs) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->drawText(y, x, text, len, attrs, line_attrs);
}

static void qtwin_draw_cursor(TermWin *, int x, int y, wchar_t *text, int len, unsigned long attrs,
                              int line_attrs) {}

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

static void qtwin_set_scrollbar(TermWin *win, int total, int start, int page) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->setScrollBar(total, start, page);
}

static void qtwin_bell(TermWin *, int mode) {}

static void qtwin_clip_write(TermWin *win, wchar_t *text, int *attrs, int len, bool must_deselect) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->writeClip(text, attrs, len, must_deselect);
}

static void qtwin_clip_request_paste(TermWin *win) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->requestPaste();
}

static void qtwin_refresh(TermWin *win) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->repaint();
}

static void qtwin_request_resize(TermWin *, int w, int h) {}

static void qtwin_set_title(TermWin *win, const char *title) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  gw->setSessionTitle(QString::fromLatin1(title));  // FIXME TODO wrong encoding
}

void qtwin_set_icon_title(TermWin *, const char *icontitle) {}

/* set_minimised and set_maximised are assumed to set two
 * independent settings, rather than a single three-way
 * {min,normal,max} switch. The idea is that when you un-minimise
 * the window it remembers whether to go back to normal or
 * maximised. */

void qtwin_set_minimised(TermWin *, bool minimised) {}

bool qtwin_is_minimised(TermWin *) { return false; }

void qtwin_set_maximised(TermWin *, bool maximised) {}

//

void qtwin_move(TermWin *, int x, int y) {}
void qtwin_set_zorder(TermWin *, bool top) {}

bool qtwin_palette_get(TermWin *, int n, int *r, int *g, int *b) { return false; }
void qtwin_palette_set(TermWin *, int n, int r, int g, int b) {}
void qtwin_palette_reset(TermWin *) {}

/*
 * Report the window's position, for terminal reports.
 */
void qtwin_get_pos(TermWin *, int *x, int *y) {
  *x = 1;
  *y = 1;
}

/*
 * Report the window's pixel size, for terminal reports.
 */
void qtwin_get_pixels(TermWin *, int *x, int *y) {
  *x = 80 * 10;
  *y = 24 * 10;
}

const char *qtwin_get_title(TermWin *, bool icon) { return icon ? "icon_name" : "window_name"; }

bool qtwin_is_utf8(TermWin *win) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  return gw->term->ucsdata->line_codepage == CP_UTF8;
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
    qtwin_is_minimised,
    qtwin_set_maximised,
    qtwin_move,
    qtwin_set_zorder,
    qtwin_palette_get,
    qtwin_palette_set,
    qtwin_palette_reset,
    qtwin_get_pos,
    qtwin_get_pixels,
    qtwin_get_title,
    qtwin_is_utf8,
};
