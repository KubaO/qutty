/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

// #define PUTTY_DO_GLOBALS /* actually _define_ globals */
#include "QtCommon.hpp"
#define SECURITY_WIN32

#include <QKeyEvent>
#include <cstdio>
#include <cstring>

#include "GuiTerminalWindow.hpp"
#include "QtTimer.hpp"

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
    this->requestPaste(CLIP_SYSTEM);
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

Filename *filename_new() {
  Filename *ret = snew(Filename);
  memset(ret, 0, sizeof(Filename));
  return ret;
}

Filename *filename_copy(const Filename *src) {
  Filename *ret = filename_new();
#if _WIN32
  size_t length = wcslen(src->wpath) + 1;
  size_t byteSize = length * sizeof(wchar_t);
  wchar_t *copy = snewn(length, wchar_t);
  memcpy(copy, src->wpath, byteSize);
  ret->wpath = copy;
#else
  size_t length = strlen(src->cpath) + 1;
  char *copy = snewn(length, char);
  memcpy(copy, src->cpath, length);
  ret->cpath = copy;
#endif
  return ret;
}

/* The str is in the system 8 bit encoding */
Filename *filename_from_str(const char *str) {
  Filename *ret = filename_new();
#if _WIN32
  QString name = QString::fromLocal8Bit(str);
  size_t length = name.length();
  size_t byteSize = length * sizeof(wchar_t);
  wchar_t *copy = snewn(length + 1, wchar_t);
  memcpy(copy, name.data(), byteSize);
  copy[length] = '\0';
  ret->wpath = copy;
#else
  size_t length = strlen(str) + 1;
  char *copy = snewn(length, char);
  memcpy(copy, str, length);
  ret->cpath = copy;
#endif
  return ret;
}

const char *filename_to_str(const Filename *cfn) {
  Filename *fn = const_cast<Filename *>(cfn);
#ifdef _WIN32
  if (!fn->cpath) {
    QStringView sv = QStringView(fn->wpath);
    QByteArray ba = sv.toLocal8Bit();
    size_t length = ba.size() + 1;
    char *copy = snewn(length, char);
    memcpy(copy, ba.constData(), length);
    fn->cpath = copy;
  }
#endif
  return fn->cpath;
}

bool filename_equal(const Filename *f1, const Filename *f2) {
#ifdef _WIN32
  return !wcscmp((const wchar_t *)f1->wpath, (const wchar_t *)f2->wpath);
#else
  return !strcmp(f1->cpath, f2->cpath);
#endif
}

bool filename_is_null(const Filename *fn) {
#ifdef _WIN32
  return !fn || !fn->wpath[0];
#else
  return !fn || !fn->cpath[0];
#endif
}

void filename_free(Filename *fn) {
  if (fn) {
    sfree((void *)fn->wpath);
    sfree((void *)fn->cpath);
    sfree(fn);
  }
}

FILE *f_open(const Filename *fn, const char *mode, bool isprivate) {
#ifdef _WIN32
  wchar_t *wmode = dup_mb_to_wc(DEFAULT_CODEPAGE, mode);
  FILE *fp = _wfopen((const wchar_t *)fn->wpath, wmode);
  sfree(wmode);
  return fp;
#else
  return fopen(fn->cpath, mode);
#endif
}

Filename *filename_from_utf8(const char *utf8) {
  QString name = QString::fromUtf8(utf8);
  return filename_from_qstring(name);
}

const char *filename_to_utf8(const Filename *fn) {
#ifdef _WIN32
  QByteArray utf8 = QStringView(fn->wpath).toUtf8();
#else
  QByteArray utf8 = QString::fromLocal8Bit(fn->cpath).toUtf8();
#endif
  size_t length = utf8.size() + 1;
  char *copy = snewn(length, char);
  memcpy(copy, utf8.constData(), length);
  return copy;
}

QString filename_to_qstring(const Filename *fn) {
#ifdef _WIN32
  return QString(fn->wpath);
#else
  return QString::fromLocal8Bit(fn->cpath);
#endif
}

Filename *filename_from_qstring(const QString &str) {
  Filename *fn = filename_new();
#ifdef _WIN32
  size_t length = str.size();
  size_t byteSize = length * sizeof(wchar_t);
  wchar_t *copy = snewn(length + 1, wchar_t);
  memcpy(copy, str.constData(), byteSize);
  copy[length] = 0;
  fn->wpath = copy;
#else
  QByteArray local = str.toLocal8Bit();
  size_t length = local.size() + 1;
  char *copy = snewn(length, char);
  memcpy(copy, local.constData(), length);
  fn->cpath = copy;
#endif
  return fn;
}

void filename_serialise(BinarySink *bs, const Filename *f) {
  const char *utf8 = filename_to_utf8(f);
  put_asciz(bs, utf8);
  sfree((void *)utf8);
}

Filename *filename_deserialise(BinarySource *src) {
  const char *utf8 = get_asciz(src);
  return filename_from_utf8(utf8);
}

struct tm ltime(void) {
  time_t rawtime;
  struct tm *timeinfo;

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  return *timeinfo;
}

extern "C" char *platform_get_x_display(void) {
  /* We may as well check for DISPLAY in case it's useful. */
  return dupstr(getenv("DISPLAY"));
}

/*
 * called to initalize tmux mode
 */
int tmux_init_tmux_mode(TermWin *win, char *tmux_version) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  return gw->initTmuxControllerMode(tmux_version);
}

size_t tmux_from_backend(TermWin *win, int is_stderr, const char *data, int len) {
  GuiTerminalWindow *gw = static_cast<GuiTerminalWindow *>(win);
  return gw->tmuxGateway()->fromBackend(is_stderr, data, len);
}

void qstring_to_char(char *dst, const QString &src, int dstlen) {
  QByteArray name = src.toUtf8();
  strncpy(dst, name.constData(), dstlen);
}

FontSpec *fontspec_new(const char *name, bool bold, int height, int charset) {
  FontSpec *f = snew(FontSpec);
  strncpy(f->name, name, sizeof(f->name));
  f->name[sizeof(f->name) - 1] = '\0';
  f->isbold = bold;
  f->height = height;
  f->charset = charset;
  return f;
}

FontSpec *fontspec_new_default(void) { return fontspec_new("", false, 0, 0); }

FontSpec *fontspec_copy(const FontSpec *f) {
  FontSpec *n = snew(FontSpec);
  memcpy(n, f, sizeof(FontSpec));
  return n;
}

void fontspec_free(FontSpec *f) { sfree(f); }

void fontspec_serialise(BinarySink *bs, FontSpec *f) {
  put_asciz(bs, f->name);
  put_uint32(bs, f->isbold);
  put_uint32(bs, f->height);
  put_uint32(bs, f->charset);
}

FontSpec *fontspec_deserialise(BinarySource *src) {
  const char *name = get_asciz(src);
  unsigned isbold = get_uint32(src);
  unsigned height = get_uint32(src);
  unsigned charset = get_uint32(src);
  return fontspec_new(name, isbold, height, charset);
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
      sprintf(es->text, "Windows error code %d (and FormatMessage returned %lu)", error,
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

#ifndef NO_SECUREZEROMEMORY
/*
 * Windows implementation of smemclr (see misc.c) using SecureZeroMemory.
 */
void smemclr(void *b, size_t n) {
  if (b && n > 0) SecureZeroMemory(b, n);
}
#endif

void qtc_assert(const char *assertion, const char *file, int line) {
  qt_assert(assertion, file, line);
}

void escape_registry_key(const char *in, strbuf *out) {
  bool candot = false;
  static const char hex[17] = "0123456789ABCDEF";

  while (*in) {
    if (*in == ' ' || *in == '\\' || *in == '*' || *in == '?' || *in == '%' || *in < ' ' ||
        *in > '~' || (*in == '.' && !candot)) {
      put_byte(out, '%');
      put_byte(out, hex[((unsigned char)*in) >> 4]);
      put_byte(out, hex[((unsigned char)*in) & 15]);
    } else
      put_byte(out, *in);
    in++;
    candot = true;
  }
}

void unescape_registry_key(const char *in, strbuf *out) {
  while (*in) {
    if (*in == '%' && in[1] && in[2]) {
      int i, j;

      i = in[1] - '0';
      i -= (i > 9 ? 7 : 0);
      j = in[2] - '0';
      j -= (j > 9 ? 7 : 0);

      put_byte(out, (i << 4) + j);
      in += 3;
    } else {
      put_byte(out, *in++);
    }
  }
}

bool open_for_write_would_lose_data(const Filename *fn) { return false; }

/*
 * Facility provided by the platform to spawn a parallel subprocess
 * and present its stdio via a Socket.
 *
 * 'prefix' indicates the prefix that should appear on messages passed
 * to plug_log to provide stderr output from the process.
 */
Socket *platform_start_subprocess(const char *cmd, Plug *plug, const char *prefix) {
  qDebug() << __FUNCTION__ << cmd << plug << prefix << "NOTIMPL";  // TODO
  return nullptr;
}

unsigned int decodeFromTerminal(Terminal *term, unsigned int uc) {
  switch (uc & CSET_MASK) {
    case CSET_LINEDRW:
      if (!term->rawcnp) {
        uc = term->ucsdata->unitab_xterm[uc & 0xFF];
        break;
      }
    case CSET_ASCII:
      uc = term->ucsdata->unitab_line[uc & 0xFF];
      break;
    case CSET_SCOACS:
      uc = term->ucsdata->unitab_scoacs[uc & 0xFF];
      break;
    case CSET_GBCHR:
      assert(false);  // term_translate should have gotten rid of those
  }
  // Yes, the above transformations may return ACP or OEMCP direct-to-font encodings
  switch (uc & CSET_MASK) {
    case CSET_ACP:
      uc = term->ucsdata->unitab_font[uc & 0xFF];
      break;
    case CSET_OEMCP:
      uc = term->ucsdata->unitab_oemcp[uc & 0xFF];
      break;
  }
  return uc;
}

int decodeFromTerminal(Terminal *term, const termline *line, QString &str, QList<bool> *advances) {
  const int prevStrLen = str.size();
  str.reserve(prevStrLen + line->size);
  if (advances) {
    advances->clear();
    advances->reserve(line->size);
  }

  for (int i = 0; i < line->cols; i++) {
    int j = i;
    do {
      const termchar &tc = line->chars[j];
      unsigned int ch = decodeFromTerminal(term, tc.chr);
      if (!QChar::requiresSurrogates(ch)) {
        str.append(QChar(ch));
        if (advances) advances->append(false);
      } else {
        str.append(QChar::highSurrogate(ch));
        str.append(QChar::lowSurrogate(ch));
        if (advances) {
          advances->append(false);
          advances->append(false);
        }
      }
      // follow runs of combining characters
      j = tc.cc_next;
      assert(j == 0 || j >= line->cols);
    } while (j);
    // advance only on the last character in a run of combining characters
    if (advances) advances->back() = true;
  }
  return str.size() - prevStrLen;
}

QString decodeFromTerminal(Terminal *term, const termline *line) {
  QString str;
  decodeFromTerminal(term, line, str);
  return str;
}

/* the entire run must have the same encoding */
QString decodeRunFromTerminal(Terminal *term, const wchar_t *text, int len) {
  QString str = QString(QStringView(text, len));
  if (str.isEmpty()) return str;

  const char16_t encoding = text[0] & CSET_MASK;

  for (int i = 1; i < len; i++) assert((text[i] & CSET_MASK) == encoding);

  switch (encoding) {
    case CSET_LINEDRW:
      if (!term->rawcnp)
        for (int i = 0; i < len; i++) str[i] = QChar(term->ucsdata->unitab_xterm[text[i] & 0xFF]);
      break;
    case CSET_ASCII:
      for (int i = 0; i < len; i++) str[i] = QChar(term->ucsdata->unitab_line[text[i] & 0xFF]);
      break;
    case CSET_SCOACS:
      for (int i = 0; i < len; i++) str[i] = QChar(term->ucsdata->unitab_scoacs[text[i] & 0xFF]);
      break;
  }
  // Yes, the above transformations may return ACP or OEMCP direct-to-font encodings
  switch (encoding) {
    case CSET_ACP:
      for (int i = 0; i < len; i++) str[i] = QChar(term->ucsdata->unitab_font[text[i] & 0xFF]);
      break;
    case CSET_OEMCP:
      for (int i = 0; i < len; i++) str[i] = QChar(term->ucsdata->unitab_oemcp[text[i] & 0xFF]);
      break;
  }
  return str;
}

#if 0
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
             const char * /*defchr*/) {
  QTextCodec *codec = getTextCodec(codepage);
  if (!codec) return 0;
  QByteArray mbarr = codec->fromUnicode(QString::fromWCharArray(wcstr, wclen));
  qstrncpy(mbstr, mbarr.constData(), mblen);
  return mbarr.length();
}
#endif
