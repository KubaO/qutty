/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include <QTextCodec>

#include "GuiTerminalWindow.hpp"
#include "QtCommon.hpp"

static bool is_utf8(TermWin *) { return true; }

static void set_title(TermWin *win, const char *title) {
  assert(win);
  GuiTerminalWindow *w = static_cast<QuttyTermWin *>(win)->w;
  w->setSessionTitle(QString::fromLatin1(title));
}

static void set_icon_title(TermWin *win, const char *icontitle) {}

static void set_scrollbar(TermWin *win, int total, int start, int page) {
  GuiTerminalWindow *w = static_cast<QuttyTermWin *>(win)->w;
  w->setScrollBar(total, start, page);
}

static bool setup_draw_ctx(TermWin *win) {
  GuiTerminalWindow *w = static_cast<QuttyTermWin *>(win)->w;
  w->preDrawTerm();
  return true;
}

static void free_draw_ctx(TermWin *win) {
  GuiTerminalWindow *w = static_cast<QuttyTermWin *>(win)->w;
  w->drawTerm();
}

/*
 * Report the window's position, for terminal reports.
 */
static void get_pos(TermWin *win, int *x, int *y) {
  *x = 1;
  *y = 1;
}

/*
 * Report the window's pixel size, for terminal reports.
 */
static void get_pixels(TermWin *win, int *x, int *y) {
  *x = 80 * 10;
  *y = 24 * 10;
}

/*
 * Return the window or icon title.
 */
static const char *get_title(TermWin *win, bool icon) { return icon ? "icon_name" : "window_name"; }

// void logtraffic(void *handle, unsigned char c, int logmode){}
// void logflush(void *handle) {}

static bool is_minimised(TermWin *) { return false; }

/* set_minimised and set_maximised are assumed to set two
 * independent settings, rather than a single three-way
 * {min,normal,max} switch. The idea is that when you un-minimise
 * the window it remembers whether to go back to normal or
 * maximised. */
static void set_minimised(TermWin *, bool minimised) {}

static void set_maximised(TermWin *, bool maximised) {}

static void request_resize(TermWin *, int w, int h) {}

void set_raw_mouse_mode(void * /*frontend*/, int /*activate*/) {}

static void bell(TermWin *, int mode) {}

static void set_zorder(TermWin *, bool top) {}

static void move(TermWin *, int x, int y) {}

static void clip_write(TermWin *win, int clipboard, wchar_t *text, int *attrs, truecolour *colours,
                       int len, bool must_deselect) {
  GuiTerminalWindow *w = static_cast<QuttyTermWin *>(win)->w;
  w->writeClip(text, attrs, colours, len, must_deselect);
}

void get_clip(void *frontend, wchar_t **p, int *len) {
  GuiTerminalWindow *f = static_cast<GuiTerminalWindow *>(frontend);
  f->getClip(p, len);
}

static void clip_request_paste(TermWin *win, int clipboard) {
  GuiTerminalWindow *w = static_cast<QuttyTermWin *>(win)->w;
  w->requestPaste();
}

static void set_raw_mouse_mode(TermWin *, bool enable) {}

static void set_cursor_pos(TermWin *, int x, int y) {}

static void refresh(TermWin *win) {
  GuiTerminalWindow *w = static_cast<QuttyTermWin *>(win)->w;
  w->repaint();
}

// int mk_wcwidth(wchar_t ucs){qDebug()<<"NOT_IMPL"<<__FUNCTION__;return 0;}
// int mk_wcwidth_cjk(wchar_t ucs){qDebug()<<"NOT_IMPL"<<__FUNCTION__;return 0;}

static void palette_set(TermWin *, int n, int r, int g, int b) {
  qDebug() << "NOT_IMPL" << __FUNCTION__;
}

static bool palette_get(TermWin *, int n, int *r, int *g, int *b) { return false; }

static void palette_reset(TermWin *) { qDebug() << "NOT_IMPL" << __FUNCTION__; }

bool is_dbcs_leadbyte(int /*codepage*/, char /*byte*/) {
  qDebug() << "NOT_IMPL" << __FUNCTION__;
  return 0;
}

static int char_width(TermWin *win, int uc) {
  if (0) qDebug() << "NOT_IMPL" << win << uc << __FUNCTION__;
  return 1;
}

/* Draw text in the window, during a painting operation */
static void draw_text(TermWin *win, int x, int y, wchar_t *text, int len, unsigned long attrs,
                      int line_attrs, truecolour tc) {
  GuiTerminalWindow *w = static_cast<QuttyTermWin *>(win)->w;
  w->drawText(x, y, text, len, attrs, line_attrs);
}

/* Draw the visible cursor. Expects you to have called do_text
 * first (because it might just draw an underline over a character
 * presumed to exist already), but also expects you to pass in all
 * the details of the character under the cursor (because it might
 * redraw it in different colours). */
static void draw_cursor(TermWin *, int x, int y, wchar_t *text, int len, unsigned long attrs,
                        int line_attrs, truecolour tc) {}

/* Draw the sigil indicating that a line of text has come from
 * PuTTY itself rather than the far end (defence against end-of-
 * authentication spoofing) */
static void draw_trust_sigil(TermWin *, int x, int y) {}

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

const TermWinVtable qutty_term_vt = {
    /*
     * All functions listed here between setup_draw_ctx and
     * free_draw_ctx expect to be _called_ between them too, so that
     * the TermWin has a drawing context currently available.
     *    * (Yes, even char_width, because e.g. the Windows implementation
     * of TermWin handles it by loading the currently configured font
     * into the HDC and doing a GDI query.)
     */
    setup_draw_ctx, draw_text,          draw_cursor,    draw_trust_sigil,
    char_width,     free_draw_ctx,

    set_cursor_pos, set_raw_mouse_mode, set_scrollbar,

    bell,

    clip_write,     clip_request_paste, refresh,

    request_resize, set_title,          set_icon_title, set_minimised,
    is_minimised,   set_maximised,      move,

    set_zorder,     palette_get,        palette_set,    palette_reset,

    get_pos,        get_pixels,         get_title,      is_utf8,
};

const SeatVtable qutty_seat_vt = {
    nullseat_output,
    nullseat_eof,
    nullseat_get_userpass_input,
    nullseat_notify_remote_exit,
    nullseat_connection_fatal,
    nullseat_update_specials_menu,
    nullseat_get_ttymode,
    nullseat_set_busy_status,
    nullseat_verify_ssh_host_key,
    nullseat_confirm_weak_crypto_primitive,
    nullseat_confirm_weak_cached_hostkey,
    nullseat_is_never_utf8,
    nullseat_echoedit_update,
    nullseat_get_x_display,
    nullseat_get_windowid,
    nullseat_get_window_pixel_size,
    nullseat_stripctrl_new,
    nullseat_set_trust_status /*_vacuously*/,
};
