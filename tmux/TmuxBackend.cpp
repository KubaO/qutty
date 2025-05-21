extern "C" {
#include "putty.h"
}
extern "C" {
#include "network.h"
}
#include <QDebug>

#include "GuiTerminalWindow.hpp"
#include "tmux/TmuxGateway.hpp"
#include "tmux/tmux.h"

struct TMux {
  Plug plug;
  Backend backend;
  TmuxGateway *gateway;
  Ldisc *ldisc;
  int paneid;
};

extern "C" void tmux_log(Plug * /*plug*/, int /*type*/, SockAddr * /*addr*/, int /*port*/,
                         const char * /*error_msg*/, int /*error_code*/) {
  qDebug() << __FUNCTION__;
}

extern "C" void tmux_closing(Plug * /*plug*/, const char * /*error_msg*/, int /*error_code*/,
                             bool /*calling_back*/) {
  qDebug() << __FUNCTION__;
}

extern "C" void tmux_receive(Plug * /*plug*/, int /*urgent*/, const char * /*data*/,
                             size_t /*len*/) {
  qDebug() << __FUNCTION__;
}

extern "C" void tmux_sent(Plug * /*plug*/, size_t /*bufsize*/) { qDebug() << __FUNCTION__; }

/*
 * Called to set up the Tmux client connection.
 *
 * Returns an error message, or NULL on success.
 */

extern "C" const char *tmux_init(Seat *seat, Backend **backend_out, LogContext * /*logctx*/,
                                 Conf * /*cfg*/, const char * /*host*/, int port,
                                 char ** /*realhost*/, bool /*nodelay*/, bool /*keepalive*/) {
  static const PlugVtable fn_table = {tmux_log, tmux_closing, tmux_receive, tmux_sent,
                                      NULL /*accepting*/};

  TMux *tmux = snew(TMux);
  memset(tmux, 0, sizeof(TMux));

  tmux->backend.vt = &tmux_client_backend;
  *backend_out = &tmux->backend;

  tmux->paneid = port;  // HACK - port is actually paneid

#if 0
  GuiTerminalWindow *termWnd = static_cast<GuiTerminalWindow *>(seat->vt->f frontend_handle);
  assert(termWnd->tmuxGateway());
  tmux->gateway = termWnd->tmuxGateway();
#endif

  return NULL;
}

extern "C" void tmux_free(Backend *be) {
  TMux *tmux = container_of(be, TMux, backend);
  sfree(tmux);
}

extern "C" void tmux_reconfig(Backend * /*be*/, Conf * /*conf*/) {}

/*
 * Called to send data down the backend connection.
 */
extern "C" size_t tmux_send(Backend *be, const char *buf, size_t len) {
  TMux *tmux = container_of(be, TMux, backend);

  const size_t wbuf_len = 20480;
  wchar_t wbuf[wbuf_len];  // for plenty of speed
  const char *rem_buf = buf;
  int i, rem_len = len;
  do {
    size_t ptrlen = 0;
    ptrlen += _snwprintf(wbuf, wbuf_len, L"send-keys -t %%%d ", tmux->paneid);
    for (i = 0; i < rem_len && ptrlen < wbuf_len - 6; i++) {
      ptrlen += _snwprintf(wbuf + ptrlen, wbuf_len - ptrlen, L"0x%02x ", rem_buf[i]);
    }
    wbuf[ptrlen - 1] = '\n';
    tmux->gateway->sendCommand(tmux->gateway, CB_NULL, wbuf, ptrlen);
    rem_buf += i;
    rem_len -= i;
  } while (rem_len > 0);
  return len;
}

/*
 * Called to query the current socket sendability status.
 */
extern "C" size_t tmux_sendbuffer(Backend * /*be*/) {
  qDebug() << __FUNCTION__;
  return 1;
}

extern "C" void tmux_unthrottle(Backend * /*be*/, size_t /*bufsize*/) {
  qDebug() << __FUNCTION__;
  // sk_set_frozen(telnet->s, backlog > TELNET_MAX_BACKLOG);
}

extern "C" bool tmux_ldisc(Backend * /*be*/, int option) {
  if (option == LD_ECHO) return FALSE;
  if (option == LD_EDIT) return FALSE;
  return FALSE;
}

extern "C" void tmux_provide_ldisc(Backend *be, Ldisc *ldisc) {
  TMux *tmux = container_of(be, TMux, backend);
  tmux->ldisc = ldisc;
}

extern "C" int tmux_cfg_info(Backend * /*be*/) { return 0; }

static const char tmux_client_backend_name[] = "tmux_client_backend";

static const BackendVtable tmux_client_backend = {tmux_init,
                                                  tmux_free,
                                                  tmux_reconfig,
                                                  tmux_send,
                                                  tmux_sendbuffer,
                                                  NULL /*size*/,
                                                  NULL /*special*/,
                                                  NULL /*get_specials*/,
                                                  NULL /*connected*/,
                                                  NULL /*exitcode*/,
                                                  NULL /*sendok*/,
                                                  tmux_ldisc,
                                                  tmux_provide_ldisc,
                                                  tmux_unthrottle,
                                                  tmux_cfg_info,
                                                  NULL /*test_for_upstream*/,
                                                  tmux_client_backend_name,
                                                  PROT_TMUX_CLIENT,
                                                  -1};
