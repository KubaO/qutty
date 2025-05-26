/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

// #define PUTTY_DO_GLOBALS /* actually _define_ globals */
#include "QtCommon.hpp"
#define SECURITY_WIN32

#include <QKeyEvent>
#include <cstring>

#include "GuiTerminalWindow.hpp"
#include "QtTimer.hpp"
extern "C" {
#include "putty.h"
#include "terminal.h"
#include "ssh.h"
}
#ifndef __linux
#include <security.h>
#include <windows.h>
#endif

using namespace Qt;

int flags;

int default_protocol = PROT_SSH;
int default_port = 22;

#ifdef NO_GSSAPI
const int ngsslibs = 3;
const struct keyvalwhere gsslibkeywords[] = {
    {(char *)"gssapi32", 0, -1, -1},
    {(char *)"sspi", 1, -1, -1},
    {(char *)"custom", 2, -1, -1},
};
#endif

// single global timer
QtTimer *globalTimer = new QtTimer;

void timer_change_notify(unsigned long next) { globalTimer->startTimerForTick(next); }

int GuiTerminalWindow::TranslateKey(QKeyEvent *keyevent, char *output) {
  Conf *conf = term->conf;
  char *p = output;

  int keystate = keyevent->modifiers();
  int ctrlshiftstate = keystate & (Qt::ControlModifier | Qt::ShiftModifier);
  int ctrlstate = keystate & Qt::ControlModifier;
  int shiftstate = keystate & Qt::ShiftModifier;
  int key = keyevent->key();

  /* Disable Auto repeat if required */
  if (term->repeat_off && keyevent->isAutoRepeat()) return 0;

  // optimization - handle it as a normal unicode key
  if (keystate == Qt::NoModifier || keystate == Qt::ShiftModifier)
    if (key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde) return -1;

  // Treat Alt by just inserting an Esc before everything else
  if (keystate & Qt::AltModifier) {
    *p++ = 0x1b;
  }

  // First some basic control characters
  if ((key == Qt::Key_Backspace) && (ctrlshiftstate == 0)) {
    // Backspace
    *p++ = (char)(term->bksp_is_delete ? 0x7F : 0x08);
    *p++ = 0;
    return -2;
  }
  if ((key == Qt::Key_Backspace) && (ctrlshiftstate == Qt::ShiftModifier)) {
    // Shift-Backspace
    *p++ = (char)(term->bksp_is_delete ? 0x08 : 0x7F);
    *p++ = 0;
    return -2;
  }
  if (((key == Qt::Key_Tab) && (ctrlshiftstate == Qt::ShiftModifier)) || (key == Qt::Key_Backtab)) {
    // Shift-Tab
    *p++ = 0x1B;
    *p++ = '[';
    *p++ = 'Z';
    return p - output;
  }
  if ((key == Qt::Key_Space) && (ctrlshiftstate & Qt::ControlModifier)) {
    // Ctrl-Space OR Ctrl-Shift-Space
    *p++ = (ctrlshiftstate & Qt::ShiftModifier) ? 160 : 0;
    return p - output;
  }
  if ((ctrlshiftstate == Qt::ControlModifier) && !(keystate & Qt::AltModifier) &&
      (key >= Qt::Key_2) && (key <= Qt::Key_8)) {
    // Ctrl-2 through Ctrl-8
    *p++ = "\000\033\034\035\036\037\177"[key - Qt::Key_2];
    return p - output;
  }
  if ((key == Qt::Key_Enter) && term->cr_lf_return) {
    // Enter (if it should send CR + LF)
    *p++ = '\r';
    *p++ = '\n';
    return p - output;
  }
  if ((keyevent->modifiers() & Qt::ControlModifier) && (keyevent->modifiers() & Qt::AltModifier)) {
    if (keyevent->text().size() >= 1) {
      /* Printing values for Ctrl + Alt + some characters
       * for diff keyboard layouts
       */
      if (keyevent->text().at(0) < 'A' || keyevent->text().at(0) > 'Z') {
        return -1;
      }
    }
  }
  if ((ctrlshiftstate == Qt::ControlModifier) && key >= Qt::Key_A && key <= Qt::Key_Z) {
    // Ctrl-a through Ctrl-z, when sent with a modifier
    *p++ = (char)(key - Qt::Key_A + 1);
    return p - output;
  }
  if ((ctrlshiftstate & Qt::ControlModifier) && (key >= Qt::Key_BracketLeft) &&
      (key <= Qt::Key_Underscore)) {
    // Ctrl-[ through Ctrl-_
    *p++ = (char)(key - Qt::Key_BracketLeft + 27);
    return p - output;
  }
  if (ctrlshiftstate == 0 && key == Qt::Key_Return) {
    // Enter
    *p++ = 0x0D;
    *p++ = 0;
    return -2;
  }

  // page-up page-down with ctrl/shift
  if (key == Key_PageUp && ctrlshiftstate) {
    if (ctrlstate && shiftstate) {  // ctrl + shift + page-up
      term_scroll_to_selection(term, 0);
    } else if (ctrlstate) {  // ctrl + page-up
      term_scroll(term, 0, -1);
    } else {  // shift + page-up
      term_scroll(term, 0, -term->rows / 2);
    }
    return 0;
  }
  if (key == Key_PageDown && ctrlshiftstate) {
    if (ctrlstate && shiftstate) {  // ctrl + shift + page-down
      term_scroll_to_selection(term, 1);
    } else if (ctrlstate) {  // ctrl + page-down
      term_scroll(term, 0, +1);
    } else {  // shift + page-down
      term_scroll(term, 0, +term->rows / 2);
    }
    return 0;
  }

  // shift-insert -> paste
  if (key == Key_Insert && ctrlshiftstate == ShiftModifier) {
    this->requestPaste();
    return 0;
  }

  // Arrows
  char xkey = 0;
  switch (key) {
    case Qt::Key_Up:
      xkey = 'A';
      break;
    case Qt::Key_Down:
      xkey = 'B';
      break;
    case Qt::Key_Right:
      xkey = 'C';
      break;
    case Qt::Key_Left:
      xkey = 'D';
      break;
    default:  // keep gcc happy
      break;
  }
  if (xkey) {
    if (term->vt52_mode)
      p += sprintf((char *)p, "\x1B%c", xkey);
    else {
      int modifier_code = -1;
      int app_flg = (term->app_cursor_keys && !term->no_applic_c);

      /* Useful mapping of Ctrl-arrows */
      if (ctrlshiftstate == Qt::ControlModifier) app_flg = !app_flg;

      if (keystate == (Qt::AltModifier | Qt::ShiftModifier))
        modifier_code = 10;
      else if (keystate == Qt::ShiftModifier)
        modifier_code = 2;
      else if (keystate == Qt::AltModifier)
        modifier_code = 3;
      else if (keystate == Qt::ControlModifier)
        modifier_code = 5;

      if (modifier_code == -1)
        return sprintf((char *)output, app_flg ? "\x1BO%c" : "\x1B[%c", xkey);
      else
        return sprintf((char *)output, "\x1B[1;%d%c", modifier_code, xkey);
    }
    return p - output;
  }

  /*
   * Next, all the keys that do tilde codes. (ESC '[' nn '~',
   * for integer decimal nn.)
   *
   * We also deal with the weird ones here. Linux VCs replace F1
   * to F5 by ESC [ [ A to ESC [ [ E. rxvt doesn't do _that_, but
   * does replace Home and End (1~ and 4~) by ESC [ H and ESC O w
   * respectively.
   */
  int code = 0;
  switch (key) {
    case Key_F1:
      code = (shiftstate ? 23 : 11);
      break;
    case Key_F2:
      code = (shiftstate ? 24 : 12);
      break;
    case Key_F3:
      code = (shiftstate ? 25 : 13);
      break;
    case Key_F4:
      code = (shiftstate ? 26 : 14);
      break;
    case Key_F5:
      code = (shiftstate ? 28 : 15);
      break;
    case Key_F6:
      code = (shiftstate ? 29 : 17);
      break;
    case Key_F7:
      code = (shiftstate ? 31 : 18);
      break;
    case Key_F8:
      code = (shiftstate ? 32 : 19);
      break;
    case Key_F9:
      code = (shiftstate ? 33 : 20);
      break;
    case Key_F10:
      code = (shiftstate ? 34 : 21);
      break;
    case Key_F11:
      code = 23;
      break;
    case Key_F12:
      code = 24;
      break;
    case Key_F13:
      code = 25;
      break;
    case Key_F14:
      code = 26;
      break;
    case Key_F15:
      code = 28;
      break;
    case Key_F16:
      code = 29;
      break;
    case Key_F17:
      code = 31;
      break;
    case Key_F18:
      code = 32;
      break;
    case Key_F19:
      code = 33;
      break;
    case Key_F20:
      code = 34;
      break;
  }
  if (ctrlstate == 0) switch (key) {
      case Key_Home:
        code = 1;
        break;
      case Key_Insert:
        code = 2;
        break;
      case Key_Delete:
        code = 3;
        break;
      case Key_End:
        code = 4;
        break;
      case Key_PageUp:
        code = 5;
        break;
      case Key_PageDown:
        code = 6;
        break;
    }
  /* Reorder edit keys to physical order */
  if (term->funky_type == FUNKY_VT400 && code <= 6) code = "\0\2\1\4\5\3\6"[code];

  if (term->vt52_mode && code > 0 && code <= 6) {
    p += sprintf((char *)p, "\x1B%c", " HLMEIG"[code]);
    return p - output;
  }

  if (term->funky_type == FUNKY_SCO && /* SCO function keys */
      code >= 11 && code <= 34) {
    char codes[] = "MNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz@[\\]^_`{";
    int index = 0;
    switch (key) {
      case Key_F1:
        index = 0;
        break;
      case Key_F2:
        index = 1;
        break;
      case Key_F3:
        index = 2;
        break;
      case Key_F4:
        index = 3;
        break;
      case Key_F5:
        index = 4;
        break;
      case Key_F6:
        index = 5;
        break;
      case Key_F7:
        index = 6;
        break;
      case Key_F8:
        index = 7;
        break;
      case Key_F9:
        index = 8;
        break;
      case Key_F10:
        index = 9;
        break;
      case Key_F11:
        index = 10;
        break;
      case Key_F12:
        index = 11;
        break;
    }
    if (shiftstate) index += 12;
    if (ctrlstate) index += 24;
    p += sprintf((char *)p, "\x1B[%c", codes[index]);
    return p - output;
  }
  if (term->funky_type == FUNKY_SCO && /* SCO small keypad */
      code >= 1 && code <= 6) {
    char codes[] = "HL.FIG";
    if (code == 3) {
      *p++ = '\x7F';
    } else {
      p += sprintf((char *)p, "\x1B[%c", codes[code - 1]);
    }
    return p - output;
  }
  if ((term->vt52_mode || term->funky_type == FUNKY_VT100P) && code >= 11 && code <= 24) {
    int offt = 0;
    if (code > 15) offt++;
    if (code > 21) offt++;
    if (term->vt52_mode)
      p += sprintf((char *)p, "\x1B%c", code + 'P' - 11 - offt);
    else
      p += sprintf((char *)p, "\x1BO%c", code + 'P' - 11 - offt);
    return p - output;
  }
  if (term->funky_type == FUNKY_LINUX && code >= 11 && code <= 15) {
    p += sprintf((char *)p, "\x1B[[%c", code + 'A' - 11);
    return p - output;
  }
  if (term->funky_type == FUNKY_XTERM && code >= 11 && code <= 14) {
    if (term->vt52_mode)
      p += sprintf((char *)p, "\x1B%c", code + 'P' - 11);
    else
      p += sprintf((char *)p, "\x1BO%c", code + 'P' - 11);
    return p - output;
  }
  if (term->rxvt_homeend && (code == 1 || code == 4)) {
    p += sprintf((char *)p, code == 1 ? "\x1B[H" : "\x1BOw");
    return p - output;
  }
  if (code) {
    p += sprintf((char *)p, "\x1B[%d~", code);
    return p - output;
  }

  // OK, handle it as a normal unicode key
  return -1;
}

Filename *filename_copy(const Filename *fn) { return filename_from_str(fn->path); }

Filename *filename_from_str(const char *str) {
  Filename *ret = snew(Filename);
  strncpy(ret->path, str, sizeof(ret->path));
  ret->path[sizeof(ret->path) - 1] = '\0';
  return ret;
}

const char *filename_to_str(const Filename *fn) { return fn->path; }

int filename_equal(const Filename *f1, const Filename *f2) { return !strcmp(f1->path, f2->path); }

int filename_is_null(const Filename *fn) { return !fn || fn->path[0] == '\0'; }

void filename_free(Filename *fn) {
  sfree(fn);
}

int filename_serialise(const Filename *f, void *vdata) {
  char *data = (char *)vdata;
  int len = strlen(f->path) + 1; /* include trailing NUL */
  if (data) {
    strcpy(data, f->path);
  }
  return len;
}

Filename *filename_deserialise(void *vdata, int maxsize, int *used) {
  char *data = (char *)vdata;
  char *end;
  end = (char *)memchr(data, '\0', maxsize);
  if (!end) return NULL;
  end++;
  *used = end - data;
  return filename_from_str(data);
}

int from_backend_untrusted(void *frontend, const char *data, int len) {
  GuiTerminalWindow *f = static_cast<GuiTerminalWindow *>(frontend);
  return term_data_untrusted(f->term, data, len);
}

struct tm ltime(void) {
  time_t rawtime;
  struct tm *timeinfo;

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  return *timeinfo;
}

char *platform_get_x_display(void) {
  /* We may as well check for DISPLAY in case it's useful. */
  return dupstr(getenv("DISPLAY"));
}

/*
 * called to initalize tmux mode
 */
int tmux_init_tmux_mode(TermWin *win, char *tmux_version) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  return gw->initTmuxControllerMode(tmux_version);
}

size_t tmux_from_backend(TermWin *win, int is_stderr, const char *data, int len) {
  GuiTerminalWindow *gw = container_of(win, GuiTerminalWindow, termwin);
  return gw->tmuxGateway()->fromBackend(is_stderr, data, len);
}

void qstring_to_char(char *dst, const QString &src, int dstlen) {
  QByteArray name = src.toUtf8();
  strncpy(dst, name.constData(), dstlen);
}

FontSpec *fontspec_new(const char *name, int bold, int height, int charset) {
  FontSpec *f = snew(FontSpec);
  strncpy(f->name, name, sizeof(f->name));
  f->name[sizeof(f->name) - 1] = '\0';
  f->isbold = bold;
  f->height = height;
  f->charset = charset;
  return f;
}

FontSpec *fontspec_copy(const FontSpec *f) {
  FontSpec *n = snew(FontSpec);
  memcpy(n, f, sizeof(FontSpec));
  return n;
}

void fontspec_free(FontSpec *f) { sfree(f); }

int fontspec_serialise(FontSpec *f, void *vdata) {
  char *data = (char *)vdata;
  int len = strlen(f->name) + 1; /* include trailing NUL */
  if (data) {
    strcpy(data, f->name);
    PUT_32BIT_MSB_FIRST(data + len, f->isbold);
    PUT_32BIT_MSB_FIRST(data + len + 4, f->height);
    PUT_32BIT_MSB_FIRST(data + len + 8, f->charset);
  }
  return len + 12; /* also include three 4-byte ints */
}

FontSpec *fontspec_deserialise(void *vdata, int maxsize, int *used) {
  char *data = (char *)vdata;
  char *end;
  if (maxsize < 13) return NULL;
  end = (char *)memchr(data, '\0', maxsize - 12);
  if (!end) return NULL;
  end++;
  *used = end - data + 12;
  return fontspec_new(data, GET_32BIT_MSB_FIRST(end), GET_32BIT_MSB_FIRST(end + 4),
                      GET_32BIT_MSB_FIRST(end + 8));
}

char *get_username(void) {
  DWORD namelen;
  char *user;
  int got_username = FALSE;
  DECL_WINDOWS_FUNCTION(static, BOOLEAN, GetUserNameExA, (EXTENDED_NAME_FORMAT, LPSTR, PULONG));

  {
    static int tried_usernameex = FALSE;
    if (!tried_usernameex) {
      /* Not available on Win9x, so load dynamically */
      HMODULE secur32 = load_system32_dll("secur32.dll");
      GET_WINDOWS_FUNCTION(secur32, GetUserNameExA);
      tried_usernameex = TRUE;
    }
  }

  if (p_GetUserNameExA) {
    /*
     * If available, use the principal -- this avoids the problem
     * that the local username is case-insensitive but Kerberos
     * usernames are case-sensitive.
     */

    /* Get the length */
    namelen = 0;
    (void)p_GetUserNameExA(NameUserPrincipal, NULL, &namelen);

    user = snewn(namelen, char);
    got_username = p_GetUserNameExA(NameUserPrincipal, user, &namelen);
    if (got_username) {
      char *p = strchr(user, '@');
      if (p) *p = 0;
    } else {
      sfree(user);
    }
  }

  if (!got_username) {
    /* Fall back to local user name */
    namelen = 0;
    if (GetUserNameA(NULL, &namelen) == FALSE) {
      /*
       * Apparently this doesn't work at least on Windows XP SP2.
       * Thus assume a maximum of 256. It will fail again if it
       * doesn't fit.
       */
      namelen = 256;
    }

    user = snewn(namelen, char);
    got_username = GetUserNameA(user, &namelen);
    if (!got_username) {
      sfree(user);
    }
  }

  return got_username ? user : NULL;
}

HMODULE load_system32_dll(const char *libname) {
  /*
   * Wrapper function to load a DLL out of c:\windows\system32
   * without going through the full DLL search path. (Hence no
   * attack is possible by placing a substitute DLL earlier on that
   * path.)
   */
  static char *sysdir = NULL;
  char *fullpath;
  HMODULE ret;

  if (!sysdir) {
    int size = 0, len;
    do {
      size = 3 * size / 2 + 512;
      sysdir = sresize(sysdir, size, char);
      len = GetSystemDirectoryA(sysdir, size);
    } while (len >= size);
  }

  fullpath = dupcat(sysdir, "\\", libname, NULL);
  ret = LoadLibraryA(fullpath);
  sfree(fullpath);
  return ret;
}

/*
 * A tree234 containing mappings from system error codes to strings.
 */

struct errstring {
  int error;
  char *text;
};

static int errstring_find(void *av, void *bv) {
  int *a = (int *)av;
  struct errstring *b = (struct errstring *)bv;
  if (*a < b->error) return -1;
  if (*a > b->error) return +1;
  return 0;
}
static int errstring_compare(void *av, void *bv) {
  struct errstring *a = (struct errstring *)av;
  return errstring_find(&a->error, bv);
}

static tree234 *errstrings = NULL;

extern "C" const char *win_strerror(int error) {
  struct errstring *es;

  if (!errstrings) errstrings = newtree234(errstring_compare);

  es = (struct errstring *)find234(errstrings, &error, errstring_find);

  if (!es) {
    int bufsize;

    es = snew(struct errstring);
    es->error = error;
    /* maximum size for FormatMessage is 64K */
    bufsize = 65535;
    es->text = snewn(bufsize, char);
    if (!FormatMessageA((FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS), NULL, error,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), es->text, bufsize, NULL)) {
      sprintf(es->text, "Windows error code %d (and FormatMessage returned %d)", error,
              GetLastError());
    } else {
      int len = strlen(es->text);
      if (len > 0 && es->text[len - 1] == '\n') es->text[len - 1] = '\0';
    }
    es->text = sresize(es->text, strlen(es->text) + 1, char);
    add234(errstrings, es);
  }

  return es->text;
}

char filename_char_sanitise(char c) {
  if (strchr("<>:\"/\\|?*", c)) return '.';
  return c;
}

/* Dummy routine, only required in plink. */
void frontend_echoedit_update(void *frontend, int echo, int edit) {}

void qtc_assert(const char *assertion, const char *file, int line) {
  qt_assert(assertion, file, line);
}
