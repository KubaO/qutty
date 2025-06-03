#include <QDebug>

#include "GuiTerminalWindow.hpp"
#include "QuTTY.hpp"
#include "tmux/TmuxGateway.hpp"
#include "tmux/tmux.h"

struct TmuxBackend {
  TmuxGateway *gateway;
  int paneid;

  Plug plug;
  Backend backend;
  LogContext *logctx;

  size_t sendbufferLength = 0;
};

void tmux_log(Plug * /*plug*/, Socket * /*socket*/, PlugLogType /*type*/, SockAddr * /*addr*/,
              int /*port*/, const char *error_msg, int /*error_code*/) {
  qDebug() << __FUNCTION__ << error_msg;
  ;
}

void tmux_closing(Plug * /*plug*/, PlugCloseType /* type */, const char *error_msg) {
  qDebug() << __FUNCTION__ << error_msg;
}

void tmux_receive(Plug * /*plug*/, int /*urgent*/, const char * /*data*/, size_t /*len*/) {
  qDebug() << __FUNCTION__;
}

void tmux_sent(Plug * /*plug*/, size_t /*bufsize*/) { qDebug() << __FUNCTION__; }

/*
 * Called to set up the Tmux client connection.
 *
 * Returns an error message, or NULL on success.
 */
char *tmux_client_init(const BackendVtable *vt, Seat *seat, Backend **backend_out,
                       LogContext *logctx, Conf * /*cfg*/, const char * /*host*/, int port,
                       char ** /*realhost*/, bool /*nodelay*/, bool /*keepalive*/) {
  static const struct PlugVtable vtable = {tmux_log, tmux_closing, tmux_receive, tmux_sent, NULL};

  GuiTerminalWindow *termWnd = container_of(seat, GuiTerminalWindow, seat);
  assert(termWnd->tmuxGateway());

  TmuxBackend *tb = new TmuxBackend();
  *backend_out = &tb->backend;
  tb->gateway = termWnd->tmuxGateway();
  tb->paneid = port;  // HACK - port is actually paneid
  tb->plug.vt = &vtable;
  tb->logctx = logctx;

  return NULL;
}

void tmux_free(Backend *be) {
  TmuxBackend *tb = container_of(be, TmuxBackend, backend);
  delete tb;
}

void tmux_reconfig(Backend * /*be*/, Conf * /*cfg*/) {}

/*
 * Called to send data down the backend connection.
 */
void tmux_send(Backend *be, const char *buf, size_t len) {
  TmuxBackend *tmuxpane = container_of(be, TmuxBackend, backend);
  const size_t wbuf_len = 20480;
  wchar_t wbuf[wbuf_len];  // for plenty of speed
  const char *rem_buf = buf;
  int i, rem_len = len;
  do {
    size_t ptrlen = 0;
    ptrlen += _snwprintf(wbuf, wbuf_len, L"send-keys -t %%%d ", tmuxpane->paneid);
    for (i = 0; i < rem_len && ptrlen < wbuf_len - 6; i++) {
      ptrlen += _snwprintf(wbuf + ptrlen, wbuf_len - ptrlen, L"0x%02x ", rem_buf[i]);
    }
    wbuf[ptrlen - 1] = '\n';
    tmuxpane->gateway->sendCommand(tmuxpane->gateway, CB_NULL, wbuf, ptrlen);
    rem_buf += i;
    rem_len -= i;
  } while (rem_len > 0);

  tmuxpane->sendbufferLength = len;
}

/*
 * Returns the current amount of buffered data.
 */
size_t tmux_sendbuffer(Backend *be) {
  TmuxBackend *tmuxpane = container_of(be, TmuxBackend, backend);
  return tmuxpane->sendbufferLength;
}

void tmux_unthrottle(Backend * /*be*/, size_t /*backlog*/) {
  qDebug() << __FUNCTION__;
  // sk_set_frozen(telnet->s, backlog > TELNET_MAX_BACKLOG);
}

bool tmux_ldisc_option_state(Backend * /*be*/, int option) {
  if (option == LD_ECHO) return false;
  if (option == LD_EDIT) return false;
  return false;
}

void tmux_provide_ldisc(Backend * /*be*/, Ldisc * /*ldisc*/) {
  // telnet->ldisc = ldisc;
}

char *tmux_close_warn_text(Backend *be) { return (char *)"tmux_close_warn_text"; }

int tmux_cfg_info(Backend * /*be*/) { return 0; }

static const char tmux_id[] = "tmux_id_NOTIMPL";
static const char tmux_client_backend_name_tc[] = "TMUX Client Backend";
static const char tmux_client_backend_name_lc[] = "tmux client backend";

BackendVtable tmux_client_backend = {tmux_client_init,
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
                                     tmux_ldisc_option_state,
                                     tmux_provide_ldisc,
                                     tmux_unthrottle,
                                     tmux_cfg_info,
                                     NULL /*test_for_upstream*/,
                                     tmux_close_warn_text,
                                     tmux_id,
                                     tmux_client_backend_name_tc,
                                     tmux_client_backend_name_lc,
                                     PROT_TMUX_CLIENT,
                                     -1};
