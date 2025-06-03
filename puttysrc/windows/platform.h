/*
 * windows/platform.h: Windows-specific inter-module stuff.
 */

#ifndef PUTTY_WINDOWS_PLATFORM_H
#define PUTTY_WINDOWS_PLATFORM_H

#include <windows.h>
#include <stdio.h>                     /* for FILENAME_MAX */

/* We use uintptr_t for Win32/Win64 portability, so we should in
 * principle include stdint.h, which defines it according to the C
 * standard. But older versions of Visual Studio don't provide
 * stdint.h at all, but do (non-standardly) define uintptr_t in
 * stddef.h. So here we try to make sure _some_ standard header is
 * included which defines uintptr_t. */
#include <stddef.h>
#if !HAVE_NO_STDINT_H
#include <stdint.h>
#endif

#include "defs.h"

#include "tree234.h"

#include "help.h"

#if defined _M_IX86 || defined _M_AMD64
#define BUILDINFO_PLATFORM "x86 Windows"
#elif defined _M_ARM || defined _M_ARM64
#define BUILDINFO_PLATFORM "Arm Windows"
#else
#define BUILDINFO_PLATFORM "Windows"
#endif

#define APPNAME "QuTTY"

struct Filename {
#ifdef _WIN32
  const wchar_t *wpath;
#endif
  const char *cpath;
};
void filename_free(Filename *);
Filename *filename_from_utf8(const char *utf8);
const char *filename_to_utf8(const Filename *fn);
FILE *f_open(const Filename *filename, const char *mode, bool isprivate);

#ifndef SUPERSEDE_FONTSPEC_FOR_TESTING
struct FontSpec {
    char name[64];
    bool isbold;
    int height;
    int charset;
};
struct FontSpec *fontspec_new(
    const char *name, bool bold, int height, int charset);
#endif

typedef struct unicode_data unicode_data_t;
    
#define PLATFORM_IS_UTF16 /* enable UTF-16 processing when exchanging
                           * wchar_t strings with environment */

#define PLATFORM_CLIPBOARDS(X)                      \
    X(CLIP_SYSTEM, "system clipboard")              \
    /* end of list */

#ifdef _WIN32
#define stricmp _stricmp
#define strnicmp _strnicmp
#endif

void qtc_assert(const char *assertion, const char *file, int line);

#undef assert
#define assert(cond) ((cond) ? (void)0 : qtc_assert(#cond, __FILE__, __LINE__))

/*
 * Dynamically linked functions. These come in two flavours:
 *
 *  - GET_WINDOWS_FUNCTION does not expose "name" to the preprocessor,
 *    so will always dynamically link against exactly what is specified
 *    in "name". If you're not sure, use this one.
 *
 *  - GET_WINDOWS_FUNCTION_PP allows "name" to be redirected via
 *    preprocessor definitions like "#define foo bar"; this is principally
 *    intended for the ANSI/Unicode DoSomething/DoSomethingA/DoSomethingW.
 *    If your function has an argument of type "LPTSTR" or similar, this
 *    is the variant to use.
 *    (However, it can't always be used, as it trips over more complicated
 *    macro trickery such as the WspiapiGetAddrInfo wrapper for getaddrinfo.)
 *
 * (DECL_WINDOWS_FUNCTION works with both these variants.)
 */
#define DECL_WINDOWS_FUNCTION(linkage, rettype, name, params)   \
    typedef rettype (WINAPI *t_##name) params;                  \
    linkage t_##name p_##name
/* If you DECL_WINDOWS_FUNCTION as extern in a header file, use this to
 * define the function pointer in a source file */
#define DEF_WINDOWS_FUNCTION(name) t_##name p_##name
#define GET_WINDOWS_FUNCTION_PP(module, name)                           \
    TYPECHECK((t_##name)NULL == name,                                   \
              (p_##name = module ?                                      \
               (t_##name) GetProcAddress(module, STR(name)) : NULL))
#define GET_WINDOWS_FUNCTION(module, name)                              \
    TYPECHECK((t_##name)NULL == name,                                   \
              (p_##name = module ?                                      \
               (t_##name) GetProcAddress(module, #name) : NULL))
#define GET_WINDOWS_FUNCTION_NO_TYPECHECK(module, name) \
    (p_##name = module ?                                \
     (t_##name) GetProcAddress(module, #name) : NULL)

#define PUTTY_REG_POS "Software\\SimonTatham\\PuTTY"
#define PUTTY_REG_PARENT "Software\\SimonTatham"
#define PUTTY_REG_PARENT_CHILD "PuTTY"
#define PUTTY_REG_GPARENT "Software"
#define PUTTY_REG_GPARENT_CHILD "SimonTatham"

/* Result values for the jumplist registry functions. */
#define JUMPLISTREG_OK 0
#define JUMPLISTREG_ERROR_INVALID_PARAMETER 1
#define JUMPLISTREG_ERROR_KEYOPENCREATE_FAILURE 2
#define JUMPLISTREG_ERROR_VALUEREAD_FAILURE 3
#define JUMPLISTREG_ERROR_VALUEWRITE_FAILURE 4
#define JUMPLISTREG_ERROR_INVALID_VALUE 5

#define GETTICKCOUNT GetTickCount
#define CURSORBLINK GetCaretBlinkTime()
#define TICKSPERSEC 1000               /* GetTickCount returns milliseconds */

#define DEFAULT_CODEPAGE CP_ACP
#define USES_VTLINE_HACK
#define CP_UTF8 65001
#define CP_437 437                     /* used for test suites */
#define CP_ISO8859_1 0x10001           /* used for test suites */

/*
 * Help file stuff in help.c.
 */
static int has_embedded_chm(void) { return 0; } /* 1 = yes, 0 = no, -1 = N/A */

/*
 * On Windows, copying to the clipboard terminates lines with CRLF.
 */
#define SEL_NL { 13, 10 }

/*
 * sk_getxdmdata() does not exist under Windows (not that I
 * couldn't write it if I wanted to, but I haven't bothered), so
 * it's a macro which always returns NULL. With any luck this will
 * cause the compiler to notice it can optimise away the
 * implementation of XDM-AUTHORIZATION-1 in ssh/x11fwd.c :-)
 */
#define sk_getxdmdata(socket, lenp) (NULL)

/*
 * Exports from network.c.
 */
/* Force a refresh of the SOCKET list by re-calling do_select for each one */
void socket_reselect_all(void);

/*
 * Exports from utils.
 */
HMODULE load_system32_dll(const char *libname);
void escape_registry_key(const char *in, strbuf *out);
void unescape_registry_key(const char *in, strbuf *out);

/*
 * Exports from unicode.c.
 */
void init_ucs(Conf *, struct unicode_data *);

/*
 * Exports from jump-list.c.
 */
static void clear_jumplist(void) {}

/*
 * Windows clipboard-UI wording.
 */
#define CLIPNAME_IMPLICIT "Last selected text"
#define CLIPNAME_EXPLICIT "System clipboard"
#define CLIPNAME_EXPLICIT_OBJECT "system clipboard"
/* These defaults are the ones PuTTY has historically had */
#define CLIPUI_DEFAULT_AUTOCOPY true
#define CLIPUI_DEFAULT_MOUSE CLIPUI_EXPLICIT
#define CLIPUI_DEFAULT_INS CLIPUI_EXPLICIT

/* In utils */
HKEY open_regkey_fn(bool create, bool write, HKEY base, const char *path, ...);
#define open_regkey_ro(base, ...) \
    open_regkey_fn(false, false, base, __VA_ARGS__, (const char *)NULL)
#define open_regkey_rw(base, ...) \
    open_regkey_fn(false, true, base, __VA_ARGS__, (const char *)NULL)
#define create_regkey(base, ...) \
    open_regkey_fn(true, true, base, __VA_ARGS__, (const char *)NULL)
void close_regkey(HKEY key);
void del_regkey(HKEY key, const char *name);
char *enum_regkey(HKEY key, int index);
bool get_reg_dword(HKEY key, const char *name, DWORD *out);
bool put_reg_dword(HKEY key, const char *name, DWORD value);
char *get_reg_sz(HKEY key, const char *name);
bool put_reg_sz(HKEY key, const char *name, const char *str);
strbuf *get_reg_multi_sz(HKEY key, const char *name);
bool put_reg_multi_sz(HKEY key, const char *name, strbuf *str);

char *get_reg_sz_simple(HKEY key, const char *name, const char *leaf);

#endif /* PUTTY_WINDOWS_PLATFORM_H */
