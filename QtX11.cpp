/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

extern "C" {
#include "putty.h"
#include "ssh.h"
}

void platform_get_x11_auth(struct X11Display *disp, Conf *cfg) {
  Filename *xauthfile = conf_get_filename(cfg, CONF_xauthfile);
  if (xauthfile->path[0]) x11_get_auth_from_authfile(disp, xauthfile->path);
}

const int platform_uses_x11_unix_by_default = FALSE;
