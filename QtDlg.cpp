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
#include "QtLogDbg.hpp"

/*
 * Ask whether the selected algorithm is acceptable (since it was
 * below the configured 'warn' threshold).
 */
int askalg(void *frontend, const char *algtype, const char *algname,
           void (*/*callback*/)(void *ctx, int result), void * /*ctx*/) {
  assert(frontend);
  GuiTerminalWindow *f = static_cast<GuiTerminalWindow *>(frontend);
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
int askappend(void * /*frontend*/, Filename *filename, void (* /*callback*/)(void *ctx, int result),
              void * /*ctx*/) {
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

struct bufchain_granule {};

int get_userpass_input_v2(void *frontend, prompts_t *p, const unsigned char *in, size_t inlen) {
  GuiTerminalWindow *f = static_cast<GuiTerminalWindow *>(frontend);
  int ret = -1;
  bufchain_granule granule = {};
  bufchain buf = {&granule, &granule, inlen, nullptr, nullptr};

  void (*queue_idempotent_callback)(IdempotentCallback *ic);
  IdempotentCallback *ic;

  // ret = cmdline_get_passwd_input(p, in, inlen);
  if (ret == -1) ret = term_get_userpass_input(f->term, p, &buf);
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

int verify_ssh_host_key(void *frontend, char *host, int port, char *keytype, char *keystr,
                        char *fingerprint, void (*/*callback*/)(void *ctx, int result),
                        void * /*ctx*/) {
  assert(frontend);
  GuiTerminalWindow *f = static_cast<GuiTerminalWindow *>(frontend);
  int ret = 1;
  QString absentmsg = QString(
                          "The server's host key is not cached in the registry. You\n"
                          "have no guarantee that the server is the computer you\n"
                          "think it is.\n"
                          "The server's " +
                          QString(keytype) + " key fingerprint is:\n") +
                      QString(fingerprint) +
                      QString(
                          "\n"
                          "If you trust this host, hit Yes to add the key to\n" APPNAME
                          "'s cache and carry on connecting.\n"
                          "If you want to carry on connecting just once, without\n"
                          "adding the key to the cache, hit No.\n"
                          "If you do not trust this host, hit Cancel to abandon the\n"
                          "connection.\n");

  QString wrongmsg = QString(
      "WARNING - POTENTIAL SECURITY BREACH!\n"
      "\n"
      "The server's host key does not match the one " APPNAME
      " has\n"
      "cached in the registry. This means that either the\n"
      "server administrator has changed the host key, or you\n"
      "have actually connected to another computer pretending\n"
      "to be the server.\n"
      "The new " +
      QString(keytype) + " key fingerprint is:\n" + QString(fingerprint) +
      "\n"
      "If you were expecting this change and trust the new key,\n"
      "hit Yes to update " APPNAME
      "'s cache and continue connecting.\n"
      "If you want to carry on connecting but without updating\n"
      "the cache, hit No.\n"
      "If you want to abandon the connection completely, hit\n"
      "Cancel. Hitting Cancel is the ONLY guaranteed safe\n"
      "choice.\n");

  /*
   * Verify the key against the registry.
   */
  ret = verify_host_key(host, port, keytype, keystr);
  if (ret == 0) /* success - key matched OK */
    return 1;
  else if (ret == 2) { /* key was different */
    switch (QMessageBox::critical(f->getMainWindow(), QString(APPNAME " Security Alert"), wrongmsg,
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                  QMessageBox::Cancel)) {
      case QMessageBox::Yes:
        store_host_key(host, port, keytype, keystr);
        return 2;
      case QMessageBox::No:
        return 1;
      default:
        return 0;
    }
  } else if (ret == 1) { /* key was absent */
    switch (QMessageBox::warning(f->getMainWindow(), QString(APPNAME " Security Alert"), absentmsg,
                                 QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                 QMessageBox::Cancel)) {
      case QMessageBox::Yes:
        store_host_key(host, port, keytype, keystr);
        return 2;
      case QMessageBox::No:
        return 1;
      default:
        return 0;
    }
  }
  return 0; /* abandon the connection */
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

void qt_message_box_no_frontend(const char *title, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  qt_vmessage_box_no_frontend(title, fmt, args);
  va_end(args);
}

void qt_vmessage_box_no_frontend(const char *title, const char *fmt, va_list args) {
  QString msg;
  QMessageBox::critical(NULL, QString(title), msg.vasprintf(fmt, args), QMessageBox::Ok);
}

void nonfatal(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  QString msg = QString::vasprintf(fmt, args);
  QMessageBox::critical(nullptr, QStringLiteral("%1 Error").arg(appname), msg, QMessageBox::Ok);
  va_end(args);
}

void qt_message_box(void *frontend, const char *title, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  qt_vmessage_box(frontend, title, fmt, args);
  va_end(args);
}

void qt_vmessage_box(void *frontend, const char *title, const char *fmt, va_list args) {
  QString msg;
  assert(frontend);
  GuiTerminalWindow *f = static_cast<GuiTerminalWindow *>(frontend);
  QMessageBox::critical(f, QString(title), msg.vasprintf(fmt, args), QMessageBox::Ok);
}

extern "C" {

void update_specials_menu(void *) {}

void connection_fatal(void *frontend, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char buf[1000];
  _snprintf(buf, sizeof(buf), fmt, args);
  qutty_connection_fatal(frontend, buf);
  va_end(args);
}

NORETURN void modalfatalbox(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  qt_vmessage_box_no_frontend(APPNAME " Fatal error", fmt, args);
  va_end(args);
  abort();
}

}  // extern "C"

void log_eventlog(LogPolicy *lp, const char *event) { qDebug() << lp << event; }

/*
 * Ask what to do about the specified output log file already
 * existing. Can return four values:
 *  *  - 2 means overwrite the log file
 *  - 1 means append to the log file
 *  - 0 means cancel logging for this session
 *  - -1 means please wait, and callback() will be called with one
 *    of those options.
 */
int log_askappend(LogPolicy *lp, Filename *filename, void (*callback)(void *ctx, int result),
                  void *ctx) {
  return 1;
}

/*
 * Emergency logging when the log file itself can't be opened,
 * which typically means we want to shout about it more loudly
 * than a mere Event Log entry.
 *  * One reasonable option is to send it to the same place that
 * stderr output from the main session goes (so, either a console
 * tool's actual stderr, or a terminal window). In many cases this
 * is unlikely to cause this error message to turn up
 * embarrassingly in a log file of real server output, because the
 * whole point is that we haven't managed to open any such log
 * file :-)
 */
void log_logging_error(LogPolicy *lp, const char *event) {}

const LogPolicyVtable qutty_logpolicy_vt = {
    log_eventlog,
    log_askappend,
    log_logging_error,
};

LogPolicy qutty_logpolicy = {&qutty_logpolicy_vt};

// from putty-0.69/windlg.c
int verify_ssh_host_key(void *frontend, char *host, int port, const char *keytype, char *keystr,
                        char *fingerprint, void (*callback)(void *ctx, int result), void *ctx) {
  int ret;
  GuiTerminalWindow *f = static_cast<GuiTerminalWindow *>(frontend);

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
int askhk(void *frontend, const char *algname, const char *betteralgs,
          void (*callback)(void *ctx, int result), void *ctx) {
  GuiTerminalWindow *f = static_cast<GuiTerminalWindow *>(frontend);
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
