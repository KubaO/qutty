/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

extern "C" {
#include "putty.h"
}

int agent_exists(void) { return FALSE; }

agent_pending_query *agent_query(void * /*in*/, int /*inlen*/, void ** /*out*/, int * /*outlen*/,
                                 void (* /*callback*/)(void *, void *, int),
                                 void * /*callback_ctx*/) {
  return NULL;
}

void agent_cancel_query(agent_pending_query *q) {
  assert(0 && "Windows agent queries are never asynchronous!");
}
