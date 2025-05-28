/*
 * Linking module for PuTTY proper: list the available backends
 */

#include <stdio.h>
#include "putty.h"
#include "tmux/tmux.h"

const char *const appname = "PuTTY";

#ifdef TELNET_DEFAULT
const int be_default_protocol = PROT_TELNET;
#else
const int be_default_protocol = PROT_SSH;
#endif

const struct BackendVtable *const backends[] = {
    &ssh_backend,
    &telnet_backend,
    &tmux_client_backend,
    NULL
};
