#ifndef QTSTUFF_H
#define QTSTUFF_H

#include <stdio.h>
#ifdef __linux
#include "unix/unix.h"
#else
#include <windows.h>
#endif
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#define stricmp _stricmp
#define strnicmp _strnicmp
#endif

#define APPNAME "QuTTY"

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

struct Filename {
    char path[FILENAME_MAX];
};

struct FontSpec {
    char name[64];
    int isbold;
    int height;
    int charset;
};
struct FontSpec *fontspec_new(const char *name, int bold, int height, int charset);

typedef uint32_t uint32;

typedef void* Context;

typedef struct unicode_data unicode_data_t;

void init_ucs(Conf *, unicode_data_t *);

#ifdef __linux
#include <time.h>
#include "unix/unix.h"
#else
#define DEFAULT_CODEPAGE CP_ACP
#endif

#define TICKSPERSEC 1000	       /* GetTickCount returns milliseconds */

#if defined _MSC_VER || defined __MINGW32__
#define GETTICKCOUNT GetTickCount

/*
 * On Windows, copying to the clipboard terminates lines with CRLF.
 */
#define SEL_NL { 13, 10 }
#endif

#define f_open(filename, mode, isprivate) (fopen((filename)->path, (mode)))

/*
 * sk_getxdmdata() does not exist under Windows (not that I
 * couldn't write it if I wanted to, but I haven't bothered), so
 * it's a macro which always returns NULL. With any luck this will
 * cause the compiler to notice it can optimise away the
 * implementation of XDM-AUTHORIZATION-1 in x11fwd.c :-)
 */
#define sk_getxdmdata(socket, lenp) (NULL)


void qt_message_box(void * frontend, const char *title, const char *fmt, ...);
void qt_message_box_no_frontend(const char *title, const char *fmt, ...);

void qt_vmessage_box(void *frontend, const char *title, const char *fmt, va_list args);
void qt_vmessage_box_no_frontend(const char *title, const char *fmt, va_list args);

#define qt_critical_msgbox(frontend, fmt, ...) \
    qt_message_box(frontend, APPNAME " Fatal Error", fmt, __VA_ARGS__)

#undef assert
#define assert(cond) do {\
    if(!(cond)) qt_message_box_no_frontend(APPNAME " Fatal Error", "fatal assert %s(%d)"#cond, __FUNCTION__, __LINE__); \
} while(0)

void qutty_connection_fatal(void *frontend, char *msg);

#ifdef __linux
#define _snprintf snprintf
#endif

static void clear_jumplist(void) {}

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
#define DECL_WINDOWS_FUNCTION(linkage, rettype, name, params) \
  typedef rettype(WINAPI *t_##name) params;                   \
  linkage t_##name p_##name
#define STR1(x) #x
#define STR(x) STR1(x)
#define GET_WINDOWS_FUNCTION_PP(module, name) \
  (p_##name = module ? (t_##name)GetProcAddress(module, STR(name)) : NULL)
#define GET_WINDOWS_FUNCTION(module, name) \
  (p_##name = module ? (t_##name)GetProcAddress(module, #name) : NULL)

HMODULE load_system32_dll(const char *libname);

#ifdef __cplusplus
}
#endif
#endif // QTSTUFF_H
