/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

extern "C" {
#include "putty.h"
#include "storage.h"
}
#undef debug  // clashes with debug in qlogging.h

#include <QMessageBox>

#include "GuiMainWindow.hpp"
#include "GuiTerminalWindow.hpp"
#include "QtConfig.hpp"

using namespace Qt::Literals::StringLiterals;

// from putty-0.80/windows/dialog.c
static const char *process_seatdialogtext(strbuf *dlg_text, const char **scary_heading,
                                          SeatDialogText *text) {
  const char *dlg_title = "";

  for (SeatDialogTextItem *item = text->items, *end = item + text->nitems; item < end; item++) {
    switch (item->type) {
      case SDT_PARA:
        put_fmt(dlg_text, "%s\r\n\r\n", item->text);
        break;
      case SDT_DISPLAY:
        put_fmt(dlg_text, "%s\r\n\r\n", item->text);
        break;
      case SDT_SCARY_HEADING:
        assert(scary_heading != NULL &&
               "only expect a scary heading if "
               "the dialog has somewhere to put it");
        *scary_heading = item->text;
        break;
      case SDT_TITLE:
        dlg_title = item->text;
        break;
      default:
        break;
    }
  }

  /* Trim any trailing newlines */
  while (strbuf_chomp(dlg_text, '\r') || strbuf_chomp(dlg_text, '\n'));

  return dlg_title;
}

/*
 * Check with the seat whether it's OK to use a cryptographic
 * primitive from below the 'warn below this line' threshold in
 * the input Conf. Return values are the same as
 * confirm_ssh_host_key above.
 */
SeatPromptResult qt_confirm_weak_crypto_primitive(Seat *seat, SeatDialogText *text,
                                                  void (*callback)(void *ctx,
                                                                   SeatPromptResult result),
                                                  void *ctx) {
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  strbuf *dlg_text = strbuf_new();
  const char *dlg_title = process_seatdialogtext(dlg_text, NULL, text);
  QString msg = dlg_text->s;
  strbuf_free(dlg_text);

  switch (QMessageBox::warning(f->getMainWindow(), QString(APPNAME " Security Alert"), msg,
                               QMessageBox::Yes | QMessageBox::No, QMessageBox::No)) {
    case QMessageBox::Yes:
      return {SPRK_OK};
    case QMessageBox::No:
      return {SPRK_USER_ABORT};
    default:
      return {SPRK_SW_ABORT};
  }
}

/*
 * Ask whether to wipe a session log file before writing to it.
 * Returns 2 for wipe, 1 for append, 0 for cancel (don't log).
 */
int qt_askappend(LogPolicy * /*lp*/, Filename *filename,
                 void (* /*callback*/)(void *ctx, int result), void * /*ctx*/) {
  // assert(frontend);
  QString msg = QString("The session log file \"") + QString(filename->path) +
                QString(
                    "\" already exists.\n"
                    "You can overwrite it with a new session log,\n"
                    "append your session log to the end of it,\n"
                    "or disable session logging for this session.\n"
                    "Hit Yes to wipe the file, No to append to it,\n"
                    "or Cancel to disable logging.");

  switch (QMessageBox::warning(NULL, QString(APPNAME " Log to File"), msg,
                               QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                               QMessageBox::Cancel)) {
    case QMessageBox::Yes:
      return 2;
    case QMessageBox::No:
      return 1;
    default:
      return 0;
  }
}

/*
 * Try to get answers from a set of interactive login prompts. The
 * prompts are provided in 'p'.
 *  * (FIXME: it would be nice to distinguish two classes of user-
 * abort action, so the user could specify 'I want to abandon this
 * entire attempt to start a session' or the milder 'I want to
 * abandon this particular form of authentication and fall back to
 * a different one' - e.g. if you turn out not to be able to
 * remember your private key passphrase then perhaps you'd rather
 * fall back to password auth rather than aborting the whole
 * session.)
 */
static SeatPromptResult qt_get_userpass_input(Seat *seat, prompts_t *p) {
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  return term_get_userpass_input(f->term, p);
}

static void hostkey_regname(char *buffer, int buffer_sz, const char *hostname, int port,
                            const char *keytype) {
  snprintf(buffer, buffer_sz, "%s@%d:%s", keytype, port, hostname);
}

void store_host_key(const char *hostname, int port, const char *keytype, const char *key) {
  char buf[200];
  hostkey_regname(buf, sizeof(buf), hostname, port, keytype);
  qutty_config.ssh_host_keys[buf] = key;
  qutty_config.saveConfig();
}

int verify_host_key(const char *hostname, int port, const char *keytype, const char *key) {
  char buf[200];
  hostkey_regname(buf, sizeof(buf), hostname, port, keytype);
  if (qutty_config.ssh_host_keys.find(buf) == qutty_config.ssh_host_keys.end()) return 1;
  if (strcmp(key, qutty_config.ssh_host_keys[buf].c_str())) return 2;
  return 0;
}

extern "C" void old_keyfile_warning(void) {
  QMessageBox::warning(NULL, QString(APPNAME "%s Key File Warning"),
                       QString(
                           "You are loading an SSH-2 private key which has an\n"
                           "old version of the file format. This means your key\n"
                           "file is not fully tamperproof. Future versions of\n" APPNAME
                           " may stop supporting this private key format,\n"
                           "so we recommend you convert your key to the new\n"
                           "format.\n"
                           "\n"
                           "You can perform this conversion by loading the key\n"
                           "into PuTTYgen and then saving it again."),
                       QMessageBox::Ok);
}

static void qt_vmessage_box(GuiTerminalWindow *frontend, const char *title, const char *fmt,
                            va_list args) {
  QString msg;
  assert(frontend);
  QMessageBox::critical(frontend, QString(title), msg.vasprintf(fmt, args), QMessageBox::Ok);
}

void qt_message_box(GuiTerminalWindow *frontend, const char *title, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  qt_vmessage_box(frontend, title, fmt, args);
  va_end(args);
}

static void qt_critical_msgbox(GuiTerminalWindow *frontend, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  qt_vmessage_box(frontend, APPNAME " Fatal Error", fmt, args);
  va_end(args);
}

void nonfatal(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  qt_vmessage_box(nullptr, APPNAME " Error", fmt, args);
  va_end(args);
}

void modalfatalbox(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  qt_vmessage_box(nullptr, APPNAME " Fatal Error", fmt, args);
  va_end(args);
  abort();
}

static void qt_connection_fatal(Seat *seat, const char *msg) {
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  if (f->userClosingTab || f->isSockDisconnected) return;

  // prevent recursive calling
  f->isSockDisconnected = true;

  qt_critical_msgbox(f, "%s", msg);

  if (conf_get_int(f->getCfg(), CONF_close_on_exit) == FORCE_ON) f->closeTerminal();
  f->setSessionTitle(f->getSessionTitle() + " (inactive)");
}

void qt_update_specials_menu(Seat *) {}

void qt_eventlog(LogPolicy *lp, const char *event) { qDebug() << lp << event; }

void qt_logging_error(LogPolicy *lp, const char *event) { qDebug() << lp << event; }

/*
 * Ask the seat whether a given SSH host key should be accepted.
 * This is called after we've already checked it by any means we
 * can do ourselves, such as checking against host key
 * fingerprints in the Conf or the host key cache on disk: once we
 * call this function, we've already decided there's nothing for
 * it but to prompt the user.
 *  * 'mismatch' reports the result of checking the host key cache:
 * it is true if the server has presented a host key different
 * from the one we expected, and false if we had no expectation in
 * the first place.
 *  * This call may prompt the user synchronously and not return
 * until the answer is available, or it may present the prompt and
 * return immediately, giving the answer later via the provided
 * callback.
 *  * Return values:
 *  *  - +1 means `user approved the key, so continue with the
 *    connection'
 *  *  - 0 means `user rejected the key, abandon the connection'
 *  *  - -1 means `I've initiated enquiries, please wait to be called
 *    back via the provided function with a result that's either 0
 *    or +1'.
 */
SeatPromptResult qt_confirm_ssh_host_key(Seat *seat, const char *host, int port,
                                         const char *keytype, char *keystr, SeatDialogText *text,
                                         HelpCtx helpctx,
                                         void (*callback)(void *ctx, SeatPromptResult result),
                                         void *ctx) {
  // TODO: c.f. HostKeyDialogProc in puttysrc/windows/dialog.c
  assert(seat);
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  int ret;

  static auto absentmsg =
      "The server's host key is not cached in the registry. You\n"
      "have no guarantee that the server is the computer you\n"
      "think it is.\n"
      "The server's %1 key fingerprint is:\n"
      "%2\n"
      "If you trust this host, hit Yes to add the key to\n"
      "%3's cache and carry on connecting.\n"
      "If you want to carry on connecting just once, without\n"
      "adding the key to the cache, hit No.\n"
      "If you do not trust this host, hit Cancel to abandon the\n"
      "connection.\n"_L1;

  static auto wrongmsg =
      "WARNING - POTENTIAL SECURITY BREACH!\n"
      "\n"
      "The server's host key does not match the one %1 has\n"
      "cached in the registry. This means that either the\n"
      "server administrator has changed the host key, or you\n"
      "have actually connected to another computer pretending\n"
      "to be the server.\n"
      "The new %2 key fingerprint is:\n"
      "%3\n"
      "If you were expecting this change and trust the new key,\n"
      "hit Yes to update %4's cache and continue connecting.\n"
      "If you want to carry on connecting but without updating\n"
      "the cache, hit No.\n"
      "If you want to abandon the connection completely, hit\n"
      "Cancel. Hitting Cancel is the ONLY guaranteed safe\n"
      "choice.\n"_L1;

  static auto mbtitle = "%1 Security Alert"_L1;

  /*
   * Verify the key against the registry.
   */
  ret = verify_host_key(host, port, keytype, keystr);

  if (ret == 0) /* success - key matched OK */
    return {SPRK_OK};

  QString caption = QString(mbtitle).arg(appname);

  if (ret == 2) {
    /* key was different */
    QString text = QString(wrongmsg).arg(appname, keytype, keystr, appname);
    auto mbret = QMessageBox::warning(f, caption, text,
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    assert(mbret == QMessageBox::Yes || mbret == QMessageBox::No || mbret == QMessageBox::Cancel);
    if (mbret == QMessageBox::Yes) {
      store_host_key(host, port, keytype, keystr);
      return {SPRK_OK};
    } else if (mbret == QMessageBox::No)
      return {SPRK_OK};
  } else if (ret == 1) {
    /* key was absent */
    QString text = QString(absentmsg).arg(keytype, keystr, appname);
    auto mbret = QMessageBox::warning(f, caption, text,
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    assert(mbret == QMessageBox::Yes || mbret == QMessageBox::No || mbret == QMessageBox::Cancel);
    if (mbret == QMessageBox::Yes) {
      store_host_key(host, port, keytype, keystr);
      return {SPRK_OK};
    } else if (mbret == QMessageBox::No)
      return {SPRK_OK};
  }
  return {SPRK_USER_ABORT}; /* abandon the connection */
}

/*
 * Variant form of confirm_weak_crypto_primitive, which prints a
 * slightly different message but otherwise has the same
 * semantics.
 *  * This form is used in the case where we're using a host key
 * below the warning threshold because that's the best one we have
 * cached, but at least one host key algorithm *above* the
 * threshold is available that we don't have cached.
 */
SeatPromptResult qt_confirm_weak_cached_hostkey(Seat *seat, SeatDialogText *text,
                                                void (*callback)(void *ctx,
                                                                 SeatPromptResult result),
                                                void *ctx) {
  strbuf *dlg_text = strbuf_new();
  const char *dlg_title = process_seatdialogtext(dlg_text, NULL, text);
  QString msg = dlg_text->s;
  strbuf_free(dlg_text);

  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  static auto const mbtitle = "%s Security Alert"_L1;
  int mbret;

  QString title = QString(mbtitle).arg(appname);
  mbret = QMessageBox::warning(f, title, msg, QMessageBox::Yes | QMessageBox::No);
#if 0
  socket_reselect_all();
#endif
  if (mbret == QMessageBox::Yes)
    return {SPRK_OK};
  else
    return {SPRK_USER_ABORT};
}

static void qt_set_busy_status(Seat * /*seat*/, BusyStatus /*status*/) {
  // TODO not implemented
  // busy_status = status;
  // update_mouse_pointer();
}

/*
 * Called when the back end wants to indicate that EOF has arrived
 * on the server-to-client stream. Returns false to indicate that
 * we intend to keep the session open in the other direction, or
 * true to indicate that if they're closing so are we.
 */
static bool qt_eof(Seat *seat) { return true; /* do respond to incoming EOF with outgoing */ }

/*
 * Called by the back end to notify that the output backlog has
 * changed size. A front end in control of the event loop won't
 * necessarily need this (they can just keep checking it via
 * backend_sendbuffer at every opportunity), but one buried in the
 * depths of something else (like an SSH proxy) will need to be
 * proactively notified that the amount of buffered data has
 * become smaller.
 */
static void qt_sent(Seat *seat, size_t new_sendbuffer) {}

/*
 * Provide authentication-banner output from the session setup.
 * End-user Seats can treat this as very similar to 'output', but
 * intermediate Seats in complex proxying situations will want to
 * implement this and 'output' differently.
 */
static size_t qt_banner(Seat *seat, const void *data, size_t len) {
  qDebug() << __FUNCTION__ << len;
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  return f->from_backend(SEAT_OUTPUT_STDOUT, (const char *)data, len);
}

/*
 * Notify the seat that the main session channel has been
 * successfully set up.
 *  * This is only used as part of the SSH proxying system, so it's
 * not necessary to implement it in all backends. A backend must
 * call this if it advertises the BACKEND_NOTIFIES_SESSION_START
 * flag, and otherwise, doesn't have to.
 */
static void qt_notify_session_started(Seat *seat) { qDebug() << __FUNCTION__; }

/*
 * Notify the seat that the process running at the other end of
 * the connection has finished.
 */
static void qt_notify_remote_exit(Seat *seat) {
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  int exitcode = backend_exitcode(f->backend);

  if (f->userClosingTab || f->isSockDisconnected) return;

  if (exitcode >= 0) {
    int close_on_exit = conf_get_int(f->getCfg(), CONF_close_on_exit);
    if (close_on_exit == FORCE_ON || (close_on_exit == AUTO && exitcode != INT_MAX)) {
      f->closeTerminal();
    } else {
      /* exitcode == INT_MAX indicates that the connection was closed
       * by a fatal error, so an error box will be coming our way and
       * we should not generate this informational one. */
      if (exitcode != INT_MAX) {
        qt_message_box(f, APPNAME " Fatal Error", "Connection closed by remote host");
        f->setSessionTitle(f->getSessionTitle() + " (inactive)");
        // prevent recursive calling
        f->isSockDisconnected = true;
      }
    }
  }
}

/*
 * Notify the seat that the whole connection has finished.
 * (Distinct from notify_remote_exit, e.g. in the case where you
 * have port forwardings still active when the main foreground
 * session goes away: then you'd get notify_remote_exit when the
 * foreground session dies, but notify_remote_disconnect when the
 * last forwarding vanishes and the network connection actually
 * closes.)
 *  * This function might be called multiple times by accident; seats
 * should be prepared to cope.
 *  * More precisely: this function notifies the seat that
 * backend_connected() might now return false where previously it
 * returned true. (Note the 'might': an accidental duplicate call
 * might happen when backend_connected() was already returning
 * false. Or even, in weird situations, when it hadn't stopped
 * returning true yet. The point is, when you get this
 * notification, all it's really telling you is that it's worth
 * _checking_ backend_connected, if you weren't already.)
 */
void qt_notify_remote_disconnect(Seat *seat) { qDebug() << __FUNCTION__; }

/*
 * Some snippets of text describing the UI actions in host key
 * prompts / dialog boxes, to be used in ssh/common.c when it
 * assembles the full text of those prompts.
 */
const SeatDialogPromptDescriptions *qt_prompt_descriptions(Seat *seat) {
  qDebug() << __FUNCTION__;
  return nullseat_prompt_descriptions(seat);
}

char *qt_get_ttymode(Seat *seat, const char *mode) {
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  return term_get_ttymode(f->term, mode);
}

bool qt_is_utf8(Seat *seat) {
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  return f->term->ucsdata->line_codepage == CP_UTF8;
}

/*
 * Provide output from the remote session. 'type' indicates the
 * type of the output (stdout or stderr), which can be used to
 * split the output into separate message channels, if the seat
 * wants to handle them differently. But combining the channels
 * into one is OK too; that's what terminal-window based seats do.
 *  * The return value is the current size of the output backlog.
 */
size_t qt_output(Seat *seat, SeatOutputType type, const void *data, size_t len) {
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  return f->from_backend(type, (const char *)data, len);
}

static const LogPolicyVtable default_logpolicy_vt = {qt_eventlog, qt_askappend, qt_logging_error};
LogPolicy default_logpolicy[1] = {&default_logpolicy_vt};

static const SeatVtable qtseat_vt = {
    qt_output,
    qt_eof,
    qt_sent,
    qt_banner,
    qt_get_userpass_input,
    qt_notify_session_started,
    qt_notify_remote_exit,
    qt_notify_remote_disconnect,
    qt_connection_fatal,
    qt_update_specials_menu,
    qt_get_ttymode,
    qt_set_busy_status,
    qt_confirm_ssh_host_key,
    qt_confirm_weak_crypto_primitive,
    qt_confirm_weak_cached_hostkey,
    qt_prompt_descriptions,
    qt_is_utf8,
    nullseat_echoedit_update,
    nullseat_get_x_display,
    nullseat_get_windowid,
    nullseat_get_window_pixel_size,
    nullseat_stripctrl_new,
    nullseat_set_trust_status,
    nullseat_can_set_trust_status_no,
    nullseat_has_mixed_input_stream_yes,
    nullseat_verbose_yes,
    nullseat_interactive_yes,
    nullseat_get_cursor_position,
};
