/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef LOG_H
#define LOG_H

extern "C" {
#include "defs.h"
}

#include <fstream>

extern std::ofstream logg;

// extern "C" {
extern const LogPolicyVtable qutty_logpolicy_vt;
extern LogPolicy qutty_logpolicy;
//}

#endif  // LOG_H
