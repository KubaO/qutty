/*
 * Common declarations for QuTTY
 * This is the QuTTY equivalent of putty.h
 */

#ifndef QUTTY_HPP
#define QUTTY_HPP

extern "C" {
#include "putty.h"
}

enum {
  PROT_TMUX_CLIENT = PROTOCOL_LIMIT,
};

#endif  // QUTTY_HPP
