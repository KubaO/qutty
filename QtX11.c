/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include "QtSsh.hpp"
#include "putty.h"
#include "ssh.h"

/*
 * QtSsh is included here to ensure that everything it declares matches with ssh.h.
 * Including ssh.h in C++ sources is not really worth it since it has to be modified
 * due to the use of the C++ keyword `new`. Several headers have to be changed,
 * and some sources need to be compiled with `-Dnew=_new`. It's best to keep PuTTY sources
 * unmodified. Thus, QtSsh.h re-defines what we need from ssh.h, and can be used from both
 * C and C++.
 */

void platform_get_x11_auth(struct X11Display *disp, Conf *cfg) {
  Filename *xauthfile = conf_get_filename(cfg, CONF_xauthfile);
  if (xauthfile->path[0]) x11_get_auth_from_authfile(disp, xauthfile->path);
}

const bool platform_uses_x11_unix_by_default = false;
