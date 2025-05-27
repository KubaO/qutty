/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

extern "C" {
#include "putty.h"
#include "ssh.h"
#include "storage.h"
}
#include <QMessageBox>

#include "GuiMainWindow.hpp"
#include "GuiTerminalWindow.hpp"
#include "QtConfig.hpp"

/*
 * Ask whether the selected algorithm is acceptable (since it was
 * below the configured 'warn' threshold).
 */
int qt_seat_confirm_weak_crypto_primitive(Seat *seat, const char *algtype, const char *algname,
                                          void (* /*callback*/)(void *ctx, int result),
                                          void * /*ctx*/) {
  assert(seat);
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  QString msg = QString("The first " + QString(algtype) +
                        " supported by the server\n"
                        "is " +
                        QString(algname) +
                        ", which is below the configured\n"
                        "warning threshold.\n"
                        "Do you want to continue with this connection?\n");
  switch (QMessageBox::warning(f->getMainWindow(), QString(APPNAME " Security Alert"), msg,
                               QMessageBox::Yes | QMessageBox::No, QMessageBox::No)) {
    case QMessageBox::Yes:
      return 2;
    case QMessageBox::No:
      return 1;
    default:
      return 0;
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

static int qt_get_userpass_input(Seat *seat, prompts_t *p, bufchain *input) {
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  int ret = -1;
  // ret = cmdline_get_passwd_input(p, in, inlen);
  if (ret == -1) ret = term_get_userpass_input(f->term, p, input);
  return ret;
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

void old_keyfile_warning(void) {
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

// from putty-0.69/windlg.c
int qt_verify_ssh_host_key(Seat *seat, const char *host, int port, const char *keytype,
                           char *keystr, char *fingerprint, void (*callback)(void *ctx, int result),
                           void *ctx) {
  assert(seat);
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  int ret;

  static const char absentmsg[] =
      "The server's host key is not cached in the registry. You\n"
      "have no guarantee that the server is the computer you\n"
      "think it is.\n"
      "The server's %s key fingerprint is:\n"
      "%s\n"
      "If you trust this host, hit Yes to add the key to\n"
      "%s's cache and carry on connecting.\n"
      "If you want to carry on connecting just once, without\n"
      "adding the key to the cache, hit No.\n"
      "If you do not trust this host, hit Cancel to abandon the\n"
      "connection.\n";

  static const char wrongmsg[] =
      "WARNING - POTENTIAL SECURITY BREACH!\n"
      "\n"
      "The server's host key does not match the one %s has\n"
      "cached in the registry. This means that either the\n"
      "server administrator has changed the host key, or you\n"
      "have actually connected to another computer pretending\n"
      "to be the server.\n"
      "The new %s key fingerprint is:\n"
      "%s\n"
      "If you were expecting this change and trust the new key,\n"
      "hit Yes to update %s's cache and continue connecting.\n"
      "If you want to carry on connecting but without updating\n"
      "the cache, hit No.\n"
      "If you want to abandon the connection completely, hit\n"
      "Cancel. Hitting Cancel is the ONLY guaranteed safe\n"
      "choice.\n";

  static const char mbtitle[] = "%s Security Alert";

  /*
   * Verify the key against the registry.
   */
  ret = verify_host_key(host, port, keytype, keystr);

  if (ret == 0) /* success - key matched OK */
    return 1;
  else if (ret == 2) { /* key was different */
    QMessageBox::StandardButton mbret;
    char *text = dupprintf(wrongmsg, appname, keytype, fingerprint, appname);
    char *caption = dupprintf(mbtitle, appname);
    mbret = QMessageBox::warning(f, caption, text,
                                 QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    assert(mbret == QMessageBox::Yes || mbret == QMessageBox::No || mbret == QMessageBox::Cancel);
    sfree(text);
    sfree(caption);
    if (mbret == QMessageBox::Yes) {
      store_host_key(host, port, keytype, keystr);
      return 1;
    } else if (mbret == QMessageBox::No)
      return 1;
  } else if (ret == 1) { /* key was absent */
    int mbret;
    char *text = dupprintf(absentmsg, keytype, fingerprint, appname);
    char *caption = dupprintf(mbtitle, appname);
    mbret = QMessageBox::warning(f, caption, text,
                                 QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    assert(mbret == QMessageBox::Yes || mbret == QMessageBox::No || mbret == QMessageBox::Cancel);
    sfree(text);
    sfree(caption);
    if (mbret == QMessageBox::Yes) {
      store_host_key(host, port, keytype, keystr);
      return 1;
    } else if (mbret == QMessageBox::No)
      return 1;
  }
  return 0; /* abandon the connection */
}

// from putty-0.69/windlg.c
int qt_seat_confirm_weak_cached_hostkey(Seat *seat, const char *algname, const char *betteralgs,
                                        void (*callback)(void *ctx, int result), void *ctx) {
  assert(seat);
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  static const char mbtitle[] = "%s Security Alert";
  static const char msg[] =
      "The first host key type we have stored for this server\n"
      "is %s, which is below the configured warning threshold.\n"
      "The server also provides the following types of host key\n"
      "above the threshold, which we do not have stored:\n"
      "%s\n"
      "Do you want to continue with this connection?\n";
  char *message, *title;
  int mbret;

  message = dupprintf(msg, algname, betteralgs);
  title = dupprintf(mbtitle, appname);
  mbret = QMessageBox::warning(f, title, message, QMessageBox::Yes | QMessageBox::No);
#if 0
  socket_reselect_all();
#endif
  sfree(message);
  sfree(title);
  if (mbret == IDYES)
    return 1;
  else
    return 0;
}

static void qt_set_busy_status(Seat * /*seat*/, BusyStatus /*status*/) {
  // TODO not implemented
  // busy_status = status;
  // update_mouse_pointer();
}

static bool qt_eof(Seat *seat) { return true; /* do respond to incoming EOF with outgoing */ }

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

char *qt_get_ttymode(Seat *seat, const char *mode) {
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  return term_get_ttymode(f->term, mode);
}

bool qt_is_utf8(Seat *seat) {
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  return win_is_utf8(&f->termwin);
}

/*
 * Provide output from the remote session. 'is_stderr' indicates
 * that the output should be sent to a separate error message
 * channel, if the seat has one. But combining both channels into
 * one is OK too; that's what terminal-window based seats do.
 *  * The return value is the current size of the output backlog.
 */
size_t qt_output(Seat *seat, bool is_stderr, const void *data, size_t len) {
  GuiTerminalWindow *f = container_of(seat, GuiTerminalWindow, seat);
  return f->from_backend(is_stderr, (const char *)data, len);
}

static const LogPolicyVtable default_logpolicy_vt = {qt_eventlog, qt_askappend, qt_logging_error};
LogPolicy default_logpolicy[1] = {&default_logpolicy_vt};

static const SeatVtable qtseat_vt = {
    qt_output,
    qt_eof,
    qt_get_userpass_input,
    qt_notify_remote_exit,
    qt_connection_fatal,
    qt_update_specials_menu,
    qt_get_ttymode,
    qt_set_busy_status,
    qt_verify_ssh_host_key,
    qt_seat_confirm_weak_crypto_primitive,
    qt_seat_confirm_weak_cached_hostkey,
    qt_is_utf8,
    nullseat_echoedit_update,
    nullseat_get_x_display,
    nullseat_get_windowid,
    nullseat_get_window_pixel_size,
    nullseat_stripctrl_new,
    nullseat_set_trust_status,
};
