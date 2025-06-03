#ifndef QTSSH_HPP
#define QTSSH_HPP

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SockAddr SockAddr;

char *platform_get_x_display(void);
SockAddr *platform_get_x11_unix_address(const char *path, int displaynum);

#ifdef __cplusplus
}
#endif

#endif  // QTSSH_HPP
