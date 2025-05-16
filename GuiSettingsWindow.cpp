/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include "GuiSettingsWindow.hpp"

#include <QAbstractButton>
#include <QColorDialog>
#include <QDebug>
#include <QFontDialog>
#include <QMessageBox>
#include <QRadioButton>
#include <QString>
#include <QVariant>

#include "GuiMainWindow.hpp"
#include "GuiTerminalWindow.hpp"
#include "QtCommon.hpp"
#include "QtConfig.hpp"
#include "serialize/QtMRUSessionList.hpp"
#include "ui_GuiSettingsWindow.h"
extern "C" {
#include "putty.h"
}

using std::map;
using std::string;
using std::stringstream;
using std::vector;

QtConfig qutty_config;

static int QUTTY_ROLE_FULL_SESSNAME = Qt::UserRole + 3;

void adjust_sessname_hierarchy(QTreeWidgetItem *item);

vector<string> qutty_string_split(const string &str, char delim) {
  stringstream ss(str);
  string tmp;
  vector<string> ret;
  while (std::getline(ss, tmp, delim)) {
    ret.push_back(tmp);
  }
  return ret;
}

GuiSettingsWindow::GuiSettingsWindow(QWidget *parent, GuiBase::SplitType openmode)
    : QDialog(parent), openMode(openmode), ui(new Ui::GuiSettingsWindow) {
  ui->setupUi(this);
  ui->stackedWidget->setCurrentIndex(0);
  ui->treeWidget->topLevelItem(0)->setData(0, Qt::UserRole, GUI_PAGE_SESSION);
  ui->treeWidget->topLevelItem(0)->child(0)->setData(0, Qt::UserRole, GUI_PAGE_LOGGING);
  ui->treeWidget->topLevelItem(1)->setData(0, Qt::UserRole, GUI_PAGE_TERMINAL);
  ui->treeWidget->topLevelItem(1)->child(0)->setData(0, Qt::UserRole, GUI_PAGE_KEYBOARD);
  ui->treeWidget->topLevelItem(1)->child(1)->setData(0, Qt::UserRole, GUI_PAGE_BELL);
  ui->treeWidget->topLevelItem(1)->child(2)->setData(0, Qt::UserRole, GUI_PAGE_FEATURES);
  ui->treeWidget->topLevelItem(2)->setData(0, Qt::UserRole, GUI_PAGE_WINDOW);
  ui->treeWidget->topLevelItem(2)->child(0)->setData(0, Qt::UserRole, GUI_PAGE_APPEARANCE);
  ui->treeWidget->topLevelItem(2)->child(1)->setData(0, Qt::UserRole, GUI_PAGE_BEHAVIOUR);
  ui->treeWidget->topLevelItem(2)->child(2)->setData(0, Qt::UserRole, GUI_PAGE_TRANSLATION);
  ui->treeWidget->topLevelItem(2)->child(3)->setData(0, Qt::UserRole, GUI_PAGE_SELECTION);
  ui->treeWidget->topLevelItem(2)->child(4)->setData(0, Qt::UserRole, GUI_PAGE_COLOURS);
  ui->treeWidget->topLevelItem(3)->setData(0, Qt::UserRole, GUI_PAGE_CONNECTION);
  ui->treeWidget->topLevelItem(3)->child(0)->setData(0, Qt::UserRole, GUI_PAGE_DATA);
  ui->treeWidget->topLevelItem(3)->child(1)->setData(0, Qt::UserRole, GUI_PAGE_PROXY);
  ui->treeWidget->topLevelItem(3)->child(2)->setData(0, Qt::UserRole, GUI_PAGE_TELNET);
  ui->treeWidget->topLevelItem(3)->child(3)->setData(0, Qt::UserRole, GUI_PAGE_RLOGIN);
  ui->treeWidget->topLevelItem(3)->child(4)->setData(0, Qt::UserRole, GUI_PAGE_SSH);
  ui->treeWidget->topLevelItem(3)->child(4)->child(0)->setData(0, Qt::UserRole, GUI_PAGE_KEX);
  ui->treeWidget->topLevelItem(3)->child(4)->child(1)->setData(0, Qt::UserRole, GUI_PAGE_AUTH);
  ui->treeWidget->topLevelItem(3)->child(4)->child(1)->child(0)->setData(0, Qt::UserRole,
                                                                         GUI_PAGE_GSSAPI);
  ui->treeWidget->topLevelItem(3)->child(4)->child(2)->setData(0, Qt::UserRole, GUI_PAGE_TTY);
  ui->treeWidget->topLevelItem(3)->child(4)->child(3)->setData(0, Qt::UserRole, GUI_PAGE_X11);
  ui->treeWidget->topLevelItem(3)->child(4)->child(4)->setData(0, Qt::UserRole, GUI_PAGE_TUNNELS);
  ui->treeWidget->topLevelItem(3)->child(4)->child(5)->setData(0, Qt::UserRole, GUI_PAGE_BUGS);
  ui->treeWidget->topLevelItem(3)->child(5)->setData(0, Qt::UserRole, GUI_PAGE_SERIAL);

  // expand all 1st level items
  ui->treeWidget->expandToDepth(0);

  ui->rb_contype_raw->setVisible(false);
  ui->rb_contype_rlogin->setVisible(false);
  ui->rb_contype_serial->setVisible(false);

  ui->gp_exit_close->setId(ui->rb_exit_always, FORCE_ON);
  ui->gp_exit_close->setId(ui->rb_exit_never, FORCE_OFF);
  ui->gp_exit_close->setId(ui->rb_exit_clean, AUTO);

  /* Options controlling session logging */

  ui->gp_seslog->setId(ui->rb_sessionlog_none, LGTYP_NONE);
  ui->gp_seslog->setId(ui->rb_sessionlog_printout, LGTYP_ASCII);
  ui->gp_seslog->setId(ui->rb_sessionlog_alloutput, LGTYP_DEBUG);
  ui->gp_seslog->setId(ui->rb_sessionlog_sshpacket, LGTYP_PACKETS);
  ui->gp_seslog->setId(ui->rb_sessionlog_rawdata, LGTYP_SSHRAW);

  ui->gp_logfile->setId(ui->rb_sessionlog_overwrite, LGXF_OVR);
  ui->gp_logfile->setId(ui->rb_sessionlog_append, LGXF_APN);
  ui->gp_logfile->setId(ui->rb_sessionlog_askuser, LGXF_ASK__);

  ui->gp_termopt_echo->setId(ui->rb_termopt_echoauto, AUTO);
  ui->gp_termopt_echo->setId(ui->rb_termopt_echoon, FORCE_ON);
  ui->gp_termopt_echo->setId(ui->rb_termopt_echooff, FORCE_OFF);

  ui->gp_termopt_edit->setId(ui->rb_termopt_editauto, AUTO);
  ui->gp_termopt_edit->setId(ui->rb_termopt_editon, FORCE_ON);
  ui->gp_termopt_edit->setId(ui->rb_termopt_editoff, FORCE_OFF);

  ui->gp_fnkeys->setId(ui->rb_fnkeys_esc, FUNKY_TILDE);
  ui->gp_fnkeys->setId(ui->rb_fnkeys_linux, FUNKY_LINUX);
  ui->gp_fnkeys->setId(ui->rb_fnkeys_xtermr6, FUNKY_XTERM);
  ui->gp_fnkeys->setId(ui->rb_fnkeys_vt400, FUNKY_VT400);
  ui->gp_fnkeys->setId(ui->rb_fnkeys_vt100, FUNKY_VT100P);
  ui->gp_fnkeys->setId(ui->rb_fnkeys_sco, FUNKY_SCO);

  ui->gp_remote_qtitle_action->setId(ui->rb_featqtitle_none, TITLE_NONE);
  ui->gp_remote_qtitle_action->setId(ui->rb_featqtitle_empstring, TITLE_EMPTY);
  ui->gp_remote_qtitle_action->setId(ui->rb_featqtitle_wndtitle, TITLE_REAL);

  ui->gp_resize_action->setId(ui->rb_wndresz_rowcolno, RESIZE_TERM);
  ui->gp_resize_action->setId(ui->rb_wndresz_fontsize, RESIZE_FONT);
  ui->gp_resize_action->setId(ui->rb_wndresz_onlywhenmax, RESIZE_EITHER);
  ui->gp_resize_action->setId(ui->rb_wndresz_forbid, RESIZE_DISABLED);

  ui->gp_addressfamily->setId(ui->rb_connectprotocol_auto, ADDRTYPE_UNSPEC);
  ui->gp_addressfamily->setId(ui->rb_connectprotocol_ipv4, ADDRTYPE_IPV4);
  ui->gp_addressfamily->setId(ui->rb_connectprotocol_ipv6, ADDRTYPE_IPV6);

  ui->gp_curappear->setId(ui->rb_curappear_block, 0);
  ui->gp_curappear->setId(ui->rb_curappear_underline, 1);
  ui->gp_curappear->setId(ui->rb_curappear_vertline, 2);

  ui->gp_fontquality->setId(ui->rb_fontappear_antialiase, FQ_ANTIALIASED);
  ui->gp_fontquality->setId(ui->rb_fontappear_nonantialiase, FQ_NONANTIALIASED);
  ui->gp_fontquality->setId(ui->rb_fontappear_default, FQ_DEFAULT);
  ui->gp_fontquality->setId(ui->rb_fontappear_clear, FQ_CLEARTYPE);

  this->loadSessionNames();

  // resize to minimum needed dimension
  this->resize(0, 0);

  // clang-format off
  this->connect(ui->l_saved_sess, SIGNAL(sig_hierarchyChanged(QTreeWidgetItem*)),
                SLOT(slot_sessname_hierarchy_changed(QTreeWidgetItem*)));
  this->connect(this, SIGNAL(rejected()), SLOT(slot_GuiSettingsWindow_rejected()));
  // clang-format on

  // set focus to hostname/ipaddress
  this->ui->le_hostname->setFocus();
}

GuiSettingsWindow::~GuiSettingsWindow() { delete ui; }

void GuiSettingsWindow::on_rb_contype_telnet_clicked() { ui->le_port->setText("23"); }

void GuiSettingsWindow::on_rb_contype_ssh_clicked() { ui->le_port->setText("22"); }

void GuiSettingsWindow::on_buttonBox_accepted() {
  if (isChangeSettingsMode) {
    emit signal_session_change(getConfig(), termWnd);
    goto cu0;
  }

  saveConfigChanges();

  if (ui->le_hostname->text() == "" &&
      ui->l_saved_sess->currentItem()->text(0) == QUTTY_DEFAULT_CONFIG_SETTINGS) {
    return;
  } else if (ui->le_hostname->text() == "") {
    QString config_name = ui->l_saved_sess->currentItem()->text(0);
    auto it = qutty_config.config_list.find(config_name);
    if (it != qutty_config.config_list.end()) setConfig(QtConfig::copy(it->second.get()));
  }
  // check for NOT_YET_SUPPORTED configs
  chkUnsupportedConfigs(getConfig());

  emit signal_session_open(getConfig(), openMode);

cu0:
  this->close();
  this->deleteLater();
}

void GuiSettingsWindow::on_buttonBox_rejected() { this->close(); }

void GuiSettingsWindow::slot_GuiSettingsWindow_rejected() {
  saveConfigChanges();
  emit signal_session_close();
  this->close();
  this->deleteLater();
}

void GuiSettingsWindow::saveConfigChanges() {
  if (!pending_session_changes) return;
  qutty_config.saveConfig();
}

#define UI_MAPPING(X)                                         \
  /* control name, keyword, value (opt) */                    \
  X("le_hostname", host)                                      \
  X("le_port", port)                                          \
  /* we could also use gb_contype etc. */                     \
  X("rb_contype_ssh", protocol, PROT_SSH)                     \
  X("rb_contype_telnet", protocol, PROT_TELNET)               \
  X("gp_exit_close", close_on_exit)                           \
  /* Session Logging */                                       \
  X("gp_seslog", logtype)                                     \
  X("le_sessionlog_filename", logfilename)                    \
  X("chb_sessionlog_flush", logflush)                         \
  X("chb_sessionlog_omitpasswd", logomitpass)                 \
  X("chb_sessionlog_omitdata", logomitdata)                   \
  /* Terminal Emulation */                                    \
  X("chb_terminaloption_autowrap", wrap_mode)                 \
  X("chb_terminaloption_decorigin", dec_om)                   \
  X("chb_terminaloption_lf", lfhascr)                         \
  X("chb_terminaloption_bgcolor", bce)                        \
  X("chb_terminaloption_blinktext", blinktext)                \
  X("le_termopt_ansback", answerback)                         \
  X("gp_termopt_echo", localecho)                             \
  X("gp_termopt_edit", localedit)                             \
  /* Keyboard Options */                                      \
  X("rb_backspacekey_ctrlh", bksp_is_delete, true)            \
  X("rb_backspace_ctrl127", bksp_is_delete, false)            \
  X("rb_homeendkeys_rxvt", rxvt_homeend, true)                \
  X("rb_homeendkeys_std", rxvt_homeend, false)                \
  X("gp_fnkeys", funky_type)                                  \
  X("rb_inicursorkeys_app", app_cursor, true)                 \
  X("rb_inicursorkeys_normal", app_cursor, false)             \
  /* nethack_keypad and app_keypad are mutually exclusive! */ \
  X("rb_ininumkeys_app", app_keypad, true)                    \
  X("rb_ininumkeys_normal", app_keypad, false)                \
  X("rb_ininumerickeys_nethack", nethack_keypad, true)        \
  X("chb_altgrkey", compose_key)                              \
  X("chb_ctrl_alt", ctrlaltkeys)                              \
  /* Terminal Features */                                     \
  X("chb_no_applic_c", no_applic_c)                           \
  X("chb_no_applic_k", no_applic_k)                           \
  X("chb_no_mouse_rep", no_mouse_rep)                         \
  X("chb_no_remote_resize", no_remote_resize)                 \
  X("chb_no_alt_screen", no_alt_screen)                       \
  X("chb_no_remote_wintitle", no_remote_wintitle)             \
  X("chb_no_dbackspace", no_dbackspace)                       \
  X("chb_no_remote_charset", no_remote_charset)               \
  X("chb_no_arabic", arabicshaping)                           \
  X("chb_no_bidi", bidi)                                      \
  X("gp_remote_qtitle_action", remote_qtitle_action)          \
  /* Window  */                                               \
  X("le_window_column", width)                                \
  X("le_window_row", height)                                  \
  X("le_wndscroll_lines", savelines)                          \
  X("chb_wndscroll_display", scrollbar)                       \
  X("chb_wndscroll_fullscreen", scrollbar_in_fullscreen)      \
  X("chb_wndscroll_resetdisply", scroll_on_disp)              \
  X("chb_wndscroll_resetkeypress", scroll_on_key)             \
  X("chb_wndscroll_pusherasedtext", erase_to_scrollback)      \
  X("gp_resize_action", resize_action)                        \
  X("gp_curappear", cursor_type)                              \
  X("chb_curblink", blink_cur)                                \
  X("gp_fontquality", font_quality)                           \
  X("chb_behaviour_warn", warn_on_close)                      \
  /* Connection  */                                           \
  X("le_ping_interval", ping_interval)                        \
  X("chb_tcp_keepalive", tcp_keepalives)                      \
  X("chb_tcp_nodelay", tcp_nodelay)                           \
  X("gp_addressfamily", addressfamily)                        \
  X("le_loghost", loghost)                                    \
  /* Connection Data  */                                      \
  X("le_datausername", username)                              \
  X("rb_datausername_systemsuse", username_from_env, true)    \
  X("rb_datausername_prompt", username_from_env, false)       \
  /* Telnet */                                                \
  X("le_termtype", termtype)                                  \
  X("le_termspeed", termspeed)                                \
  /* SSH Options */                                           \
  X("le_remote_cmd", remote_cmd)                              \
  /* SSH Auth  */                                             \
  X("chb_ssh_no_userauth", ssh_no_userauth)                   \
  X("chb_ssh_show_banner", ssh_show_banner)                   \
  X("chb_ssh_tryagent", tryagent)                             \
  X("chb_ssh_try_tis_auth", try_tis_auth)                     \
  X("chb_ssh_try_ki_auth", try_ki_auth)                       \
  X("chb_ssh_agentfwd", agentfwd)                             \
  X("chb_ssh_change_username", change_username)               \
  X("le_ssh_auth_keyfile", keyfile)

/*
 * These options are yet to be incorporated into the UI.
 * They are here to ensure they are valid CONF_ keys,
 * keeping the scope of work known.
 */
#define UI_MAPPING_TODO(X)                 \
  /* control name, keyword, value (opt) */ \
  /* Proxy options */                      \
  X("", proxy_exclude_list)                \
  X("", proxy_dns)                         \
  X("", even_proxy_localhost)              \
  X("", proxy_type)                        \
  X("", proxy_host)                        \
  X("", proxy_port)                        \
  X("", proxy_username)                    \
  X("", proxy_password)                    \
  X("", proxy_telnet_command)              \
  /* SSH options */                        \
  X("", remote_cmd2)                       \
  X("", nopty)                             \
  X("", compression)                       \
  X("", ssh_kexlist)                       \
  X("", ssh_rekey_time)                    \
  X("", ssh_rekey_data)                    \
  X("", ssh_cipherlist)                    \
  X("", sshprot)                           \
  X("", ssh2_des_cbc)                      \
  X("", try_gssapi_auth)                   \
  X("", gssapifwd)                         \
  X("", ssh_gsslist)                       \
  X("", ssh_gss_custom)                    \
  X("", ssh_subsys)                        \
  X("", ssh_subsys2)                       \
  X("", ssh_no_shell)                      \
  X("", ssh_nc_host)                       \
  X("", ssh_nc_port)                       \
  /* Telnet */                             \
  X("", ttymodes)                          \
  X("", environmt)                         \
  X("", localusername)                     \
  X("", rfc_environ)                       \
  X("", passive_telnet)                    \
  /* Serial Port  */                       \
  X("", serline)                           \
  X("", serspeed)                          \
  X("", serdatabits)                       \
  X("", serstopbits)                       \
  X("", serparity)                         \
  X("", serflow)                           \
  /* Keyboard */                           \
  X("", telnet_keyboard)                   \
  X("", telnet_newline)                    \
  X("", alt_f4)                            \
  X("", alt_space)                         \
  X("", alt_only)                          \
  X("", alwaysontop)                       \
  X("", fullscreenonaltenter)              \
  X("", wintitle)                          \
  /* Terminal */                           \
  X("", beep)                              \
  X("", beep_ind)                          \
  X("", bellovl)                           \
  X("", bellovl_n)                         \
  X("", bellovl_t)                         \
  X("", bellovl_s)                         \
  X("", bell_wavefile)                     \
  X("", bce)                               \
  X("", win_name_always)                   \
  X("", hide_mouseptr)                     \
  X("", sunken_edge)                       \
  X("", window_border)                     \
  X("", printer)                           \
  /* Colour  */                            \
  X("", ansi_colour)                       \
  X("", xterm_256_colour)                  \
  X("", system_colour)                     \
  X("", try_palette)                       \
  X("", bold_style)                        \
  /* Selection  */                         \
  X("", mouse_is_xterm)                    \
  X("", rect_select)                       \
  X("", rawcnp)                            \
  X("", rtf_paste)                         \
  X("", mouse_override)                    \
  X("", wordness)                          \
  /* translations */                       \
  X("", vtmode)                            \
  X("", cjk_ambig_wide)                    \
  X("", utf8_override)                     \
  X("", xlat_capslockcyr)                  \
  /* X11 */                                \
  X("", x11_forward)                       \
  X("", x11_display)                       \
  X("", x11_auth)                          \
  X("", xauthfile)                         \
  /* Port Forwarding */                    \
  X("", lport_acceptall)                   \
  X("", rport_acceptall)                   \
  X("", portfwd)                           \
  /* SSH Bug Compatibility Modes */        \
  X("", sshbug_ignore1)                    \
  X("", sshbug_plainpw1)                   \
  X("", sshbug_rsa1)                       \
  X("", sshbug_hmac2)                      \
  X("", sshbug_derivekey2)                 \
  X("", sshbug_rsapad2)                    \
  X("", sshbug_pksessid2)                  \
  X("", sshbug_rekey2)                     \
  X("", sshbug_maxpkt2)                    \
  X("", sshbug_ignore2)                    \
  X("", sshbug_winadj)                     \
  X("", ssh_simple)                        \
  /* Pterm */                              \
  X("", stamp_utmp)                        \
  X("", login_shell)                       \
  X("", scrollbar_on_left)                 \
  X("", shadowbold)                        \
  X("", boldfont)                          \
  X("", widefont)                          \
  X("", wideboldfont)                      \
  X("", shadowboldoffset)                  \
  X("", crhaslf)                           \
  X("", winclass)

#define CONF_VALIDATOR(name, key, ...) key = CONF_##key,
enum class UI_MAPPING_TODO_VALID { UI_MAPPING_TODO(CONF_VALIDATOR) };

#define UI_TO_KEY_ENTRY(ui, key, ...) {ui, {CONF_##key __VA_OPT__(, ) __VA_ARGS__}},
struct KeyValue {
  config_primary_key key;
  int value = -1;
  bool is_bool = false;

  KeyValue(config_primary_key key, bool value) : key(key), value(int(value)), is_bool(true) {}
  KeyValue(config_primary_key key, int value) : key(key), value(value) {}
  KeyValue(config_primary_key key) : key(key) {}
};

static const std::unordered_map<std::string, KeyValue> UI_TO_KV = {UI_MAPPING(UI_TO_KEY_ENTRY)};

enum Type {
  TYPE_NONE,
  TYPE_STR,
  TYPE_INT,
  TYPE_FILENAME,
  TYPE_FONT,
};

#define KEY_TO_TYPE_ENTRY(value, subkey, key) {CONF_##key, TYPE_##value},
static const std::unordered_map<config_primary_key, Type> KEY_TO_TYPE = {
    CONFIG_OPTIONS(KEY_TO_TYPE_ENTRY)};

void GuiSettingsWindow::setConfig(QtConfig::Pointer &&_cfg) {
  int ind;

  this->cfg = std::move(_cfg);

  // update the ui with the given settings

  for (QWidget *widget : findChildren<QWidget *>()) {
    auto ikv = UI_TO_KV.find(widget->objectName().toStdString());
    if (ikv == UI_TO_KV.end()) continue;

    const KeyValue &kv = ikv->second;
    config_primary_key const key = kv.key;
    int const value = kv.value;
    Type const type = KEY_TO_TYPE.at(key);

    QCheckBox *cb = qobject_cast<QCheckBox *>(widget);
    if (cb) {
      assert(type == TYPE_INT);
      cb->setChecked(conf_get_int(cfg.get(), key));
      continue;
    }
    QLineEdit *le = qobject_cast<QLineEdit *>(widget);
    if (le) {
      if (type == TYPE_STR)
        le->setText(conf_get_str(cfg.get(), key));
      else if (type == TYPE_INT)
        le->setText(QString::number(conf_get_int(cfg.get(), key)));
      else if (type == TYPE_FILENAME)
        le->setText(conf_get_filename(cfg.get(), key)->path);
      continue;
    }
    QButtonGroup *bg = qobject_cast<QButtonGroup *>(widget);
    if (bg) {
      assert(type == TYPE_INT);
      bg->button(conf_get_int(cfg.get(), key))->click();
      continue;
    }
    QRadioButton *rb = qobject_cast<QRadioButton *>(widget);
    if (rb) {
      assert(type == TYPE_INT);
      int cv = conf_get_int(cfg.get(), key);
      if (kv.is_bool) {
        if (cv && value || !cb && !value) rb->click();
      } else {
        if (cv == value) rb->click();
      }
      continue;
    }
  }

  if (0) {  // FIXME - does this matter?
    char *host = conf_get_str(cfg.get(), CONF_host);
    if (host[0] != '\0') ui->le_hostname->setText(host);
  }

  char *config_name = conf_get_str(cfg.get(), CONF_config_name);
  auto cfg_name_split = qutty_string_split(string(config_name), QUTTY_SESSION_NAME_SPLIT);
  ui->le_saved_sess->setText(QString::fromStdString(cfg_name_split.back()));

  QList<QTreeWidgetItem *> sel_saved_sess =
      ui->l_saved_sess->findItems(config_name, Qt::MatchExactly);
  if (sel_saved_sess.size() > 0) ui->l_saved_sess->setCurrentItem(sel_saved_sess[0]);

  /* Options controlling session logging */
  if (conf_get_int(cfg.get(), CONF_logxfovr) == LGXF_ASK)  // handle -ve value
    ui->gp_logfile->button(LGXF_ASK__)->click();
  else
    ui->gp_logfile->button(conf_get_int(cfg.get(), CONF_logxfovr))->click();
  ui->chb_sessionlog_flush->setChecked(true);

  /* window options */
  FontSpec &font = *conf_get_fontspec(cfg.get(), CONF_font);
  ui->lbl_fontsel->setText(
      QString("%1, %2%3-point")
          .arg(font.name, font.isbold ? "Bold, " : "", QString::number(font.height)));
  ind = ui->cb_codepage->findText(conf_get_str(cfg.get(), CONF_line_codepage));
  if (ind == -1) ind = 0;
  ui->cb_codepage->setCurrentIndex(ind);
}

static void getChecked(Conf *conf, config_primary_key key, QAbstractButton *src) {
  conf_set_int(conf, key, src->isChecked() ? 1 : 0);
}

static void getCheckedId(Conf *conf, config_primary_key key, QButtonGroup *src) {
  conf_set_int(conf, key, src->checkedId());
}

static void getText(Conf *conf, config_primary_key key, QLineEdit *src) {
  QByteArray text = src->text().toUtf8();
  conf_set_str(conf, key, text.data());
}

static void getTextVariant(Conf *conf, config_primary_key key, const QVariant &var) {
  QByteArray text = var.toString().toUtf8();
  conf_set_str(conf, key, text);
}

static void getTextPath(Conf *conf, config_primary_key key, QLineEdit *src) {
  Filename *fn = conf_get_filename(conf, key);
  qstring_to_char(fn->path, src->text(), sizeof(fn->path));
  conf_set_filename(conf, key, fn);
}

static void getTextAsNumber(Conf *conf, config_primary_key key, QLineEdit *src) {
  QByteArray text = src->text().toUtf8();
  conf_set_int(conf, key, text.toInt());
}

Conf *GuiSettingsWindow::getConfig() {
  Conf *cfg = this->cfg.get();

  // update the config with current ui selection and return it

  for (QWidget *const widget : findChildren<QWidget *>()) {
    auto ikv = UI_TO_KV.find(widget->objectName().toStdString());
    if (ikv == UI_TO_KV.end()) continue;

    const KeyValue &kv = ikv->second;
    config_primary_key const key = kv.key;
    int const value = kv.value;
    Type const type = KEY_TO_TYPE.at(key);

    QCheckBox *cb = qobject_cast<QCheckBox *>(widget);
    if (cb) {
      assert(type == TYPE_INT);
      conf_set_int(cfg, key, cb->isChecked());
      continue;
    }
    QLineEdit *le = qobject_cast<QLineEdit *>(widget);
    if (le) {
      if (type == TYPE_STR)
        conf_set_str(cfg, key, le->text().toUtf8());
      else if (type == TYPE_INT) {
        bool ok;
        int i = le->text().toInt(&ok);
        if (ok) conf_set_int(cfg, key, i);
      } else if (type == TYPE_FILENAME) {
        Filename fn = *conf_get_filename(cfg, key);
        qstring_to_char(fn.path, le->text(), sizeof(fn.path));
        conf_set_filename(cfg, key, &fn);
      }
      continue;
    }
    QButtonGroup *bg = qobject_cast<QButtonGroup *>(widget);
    if (bg) {
      assert(type == TYPE_INT);
      conf_set_int(cfg, key, bg->checkedId());
      continue;
    }
    QRadioButton *rb = qobject_cast<QRadioButton *>(widget);
    if (rb) {
      assert(type == TYPE_INT);
      if (rb->isChecked()) conf_set_int(cfg, key, value);
      continue;
    }
  }

  if (ui->l_saved_sess->currentItem())
    getTextVariant(cfg, CONF_config_name,
                   ui->l_saved_sess->currentItem()->data(0, QUTTY_ROLE_FULL_SESSNAME));

  /* Session Logging */
  getCheckedId(cfg, CONF_logxfovr, ui->gp_logfile);

  /* Window */
  conf_set_str(cfg, CONF_line_codepage, ui->cb_codepage->currentText().toUtf8());

  return cfg;
}

void GuiSettingsWindow::loadSessionNames() {
  map<QString, QTreeWidgetItem *> folders;
  folders[""] = ui->l_saved_sess->invisibleRootItem();
  ui->l_saved_sess->clear();
  for (auto it = qutty_config.config_list.begin(); it != qutty_config.config_list.end(); it++) {
    QString fullsessname = it->first;
    if (folders.find(fullsessname) != folders.end()) continue;
    if (fullsessname.endsWith(QUTTY_SESSION_NAME_SPLIT)) fullsessname.chop(1);
    QStringList split = fullsessname.split(QUTTY_SESSION_NAME_SPLIT);
    QString sessname = split.back();
    QString dirname = fullsessname.mid(0, fullsessname.lastIndexOf(QUTTY_SESSION_NAME_SPLIT));
    if (dirname == fullsessname) dirname = "";
    if (folders.find(dirname) == folders.end()) {
      QTreeWidgetItem *par = ui->l_saved_sess->invisibleRootItem();
      QString tmpdir = "";
      for (int i = 0; i < split.size() - 1; i++) {
        tmpdir += split[i];
        if (folders.find(tmpdir) == folders.end()) {
          QTreeWidgetItem *newitem = new QTreeWidgetItem(par);
          newitem->setText(0, split[i]);
          newitem->setData(0, QUTTY_ROLE_FULL_SESSNAME, tmpdir);
          folders[tmpdir] = newitem;
          par = newitem;
        } else
          par = folders[tmpdir];
        tmpdir += QUTTY_SESSION_NAME_SPLIT;
      }
    }
    QTreeWidgetItem *item = new QTreeWidgetItem(folders[dirname]);
    item->setText(0, sessname);
    item->setData(0, QUTTY_ROLE_FULL_SESSNAME, fullsessname);
    if (fullsessname == QUTTY_DEFAULT_CONFIG_SETTINGS) {
      item->setFlags(item->flags() ^ Qt::ItemIsDragEnabled);
      item->setFlags(item->flags() ^ Qt::ItemIsDropEnabled);
    }
    folders[fullsessname] = item;
  }
}

void GuiSettingsWindow::on_l_saved_sess_currentItemChanged(QTreeWidgetItem *current,
                                                           QTreeWidgetItem * /*previous*/) {
  if (isChangeSettingsMode) return;
  if (!current) return;
  QString config_name;
  config_name = current->data(0, QUTTY_ROLE_FULL_SESSNAME).toString();
  auto it = qutty_config.config_list.find(config_name);
  if (it != qutty_config.config_list.end()) setConfig(QtConfig::copy(it->second.get()));
  ui->le_saved_sess->setText(current->text(0));
}

void GuiSettingsWindow::on_b_save_sess_clicked() {
  QTreeWidgetItem *item = ui->l_saved_sess->currentItem();
  QString oldfullname, fullname, name;
  if (!item) return;
  if (item->text(0) == QUTTY_DEFAULT_CONFIG_SETTINGS) {
    name = fullname = QUTTY_DEFAULT_CONFIG_SETTINGS;
  } else {
    name = ui->le_saved_sess->text();
    if (!name.size()) return;
    if (item->parent())
      fullname =
          item->parent()->data(0, QUTTY_ROLE_FULL_SESSNAME).toString() + QUTTY_SESSION_NAME_SPLIT;
    fullname.append(name);
  }
  oldfullname = item->data(0, QUTTY_ROLE_FULL_SESSNAME).toString();
  qutty_config.config_list.erase(oldfullname);
  QtConfig::Pointer cfg = QtConfig::copy(this->getConfig());
  conf_set_str(cfg.get(), CONF_config_name, fullname.toUtf8());
  qutty_config.config_list[fullname] = std::move(cfg);

  item->setText(0, name);
  item->setData(0, QUTTY_ROLE_FULL_SESSNAME, fullname);

  for (int i = 0; i < item->childCount(); i++) adjust_sessname_hierarchy(item->child(i));

  pending_session_changes = true;
}

void GuiSettingsWindow::loadInitialSettings(Conf *cfg) {
  char *config_name = conf_get_str(cfg, CONF_config_name);
  if (qutty_config.config_list.find(config_name) != qutty_config.config_list.end()) {
    setConfig(QtConfig::copy(qutty_config.config_list[config_name].get()));
    vector<string> split = qutty_string_split(config_name, QUTTY_SESSION_NAME_SPLIT);
    string sessname = split.back();
    ui->le_saved_sess->setText(QString::fromStdString(sessname));
  }
}

void GuiSettingsWindow::enableModeChangeSettings(QtConfig::Pointer &&cfg,
                                                 GuiTerminalWindow *termWnd) {
  isChangeSettingsMode = true;
  this->termWnd = termWnd;
  setConfig(std::move(cfg));

  ui->buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  ui->gp_connection->setEnabled(false);
  ui->b_delete_sess->setEnabled(false);
  ui->b_sess_copy->setEnabled(false);
  ui->l_saved_sess->setEnabled(false);
}

void GuiSettingsWindow::on_b_delete_sess_clicked() {
  QTreeWidgetItem *delitem = ui->l_saved_sess->currentItem();
  QString config_name;
  if (!delitem || delitem->text(0) == QUTTY_DEFAULT_CONFIG_SETTINGS) return;
  if (delitem->childCount()) {
    QMessageBox::information(this, tr("Cannot delete session"),
                             tr("First delete the child sessions"));
    return;
  }
  config_name = delitem->data(0, QUTTY_ROLE_FULL_SESSNAME).toString();
  qutty_mru_sesslist.deleteSession(config_name);
  qutty_config.config_list.erase(config_name);
  delete delitem;

  pending_session_changes = true;
}

void GuiSettingsWindow::on_l_saved_sess_doubleClicked(const QModelIndex &) {
  if (isChangeSettingsMode) return;

  if (!ui->le_hostname->text().length()) return;
  QTreeWidgetItem *item = ui->l_saved_sess->currentItem();
  if (!item) return;
  if (item->childCount())  // allow double-click to expand
    return;
  on_buttonBox_accepted();
}

void adjust_sessname_hierarchy(QTreeWidgetItem *item) {
  if (!item) return;
  QTreeWidgetItem *par = item->parent();
  QString fullname, oldfullname;
  if (par) fullname = par->data(0, QUTTY_ROLE_FULL_SESSNAME).toString() + QUTTY_SESSION_NAME_SPLIT;
  fullname += item->text(0);
  oldfullname = item->data(0, QUTTY_ROLE_FULL_SESSNAME).toString();
  if (fullname == oldfullname) return;  // no change
  item->setData(0, QUTTY_ROLE_FULL_SESSNAME, fullname);
  QtConfig::Pointer cfg = std::move(qutty_config.config_list[oldfullname]);
  QByteArray _fullname = fullname.toUtf8();
  conf_set_str(cfg.get(), CONF_config_name, _fullname.data());
  qutty_config.config_list.erase(oldfullname);
  qutty_config.config_list[fullname] = std::move(cfg);
  for (int i = 0; i < item->childCount(); i++) adjust_sessname_hierarchy(item->child(i));
}

void GuiSettingsWindow::slot_sessname_hierarchy_changed(QTreeWidgetItem *item) {
  // current item & its child have moved to different place
  adjust_sessname_hierarchy(item);
  pending_session_changes = true;
}

void GuiSettingsWindow::on_btn_ssh_auth_browse_keyfile_clicked() {
  ui->le_ssh_auth_keyfile->setText(QFileDialog::getOpenFileName(
      this, tr("Select private key file"), ui->le_ssh_auth_keyfile->text(), tr("*.ppk")));
}

void GuiSettingsWindow::on_btn_fontsel_clicked() {
  FontSpec &font = *conf_get_fontspec(cfg.get(), CONF_font);
  QFont oldfont = QFont(font.name, font.height);
  oldfont.setBold(font.isbold);
  QFont selfont = QFontDialog::getFont(NULL, oldfont);

  qstring_to_char(font.name, selfont.family(), sizeof(font.name));
  font.height = selfont.pointSize();
  font.isbold = selfont.bold();
  ui->lbl_fontsel->setText(
      QString("%1, %2%3-point")
          .arg(font.name, font.isbold ? "Bold, " : "", QString::number(font.height)));
  ui->lbl_fontsel_varpitch->setText(
      selfont.fixedPitch() ? "The selected font has variable-pitch. Doesn't have fixed-pitch" : "");
}

void GuiSettingsWindow::on_treeWidget_itemSelectionChanged() {
  ui->stackedWidget->setCurrentIndex(
      ui->treeWidget->selectedItems()[0]->data(0, Qt::UserRole).toInt());
}

void GuiSettingsWindow::on_btn_about_clicked() {
  QMessageBox::about(this, "About " APPNAME, APPNAME "\nRelease " QUTTY_RELEASE_VERSION
                                                     "\n\nhttp://code.google.com/p/qutty/");
}

void GuiSettingsWindow::on_btn_colour_modify_clicked() {
  QColor oldcol = QColor(ui->le_colour_r->text().toInt(), ui->le_colour_g->text().toInt(),
                         ui->le_colour_b->text().toInt());
  QColor newcol = QColorDialog::getColor(oldcol);
  if (newcol.isValid()) {
    ui->le_colour_r->setText(QString::number(newcol.red()));
    ui->le_colour_g->setText(QString::number(newcol.green()));
    ui->le_colour_b->setText(QString::number(newcol.blue()));
    int currind = ui->l_colour->currentIndex().row();
    if (currind >= 0 && currind < NCFGCOLOURS) {
      QRgb rgba = newcol.rgba();  // FIXME is this right?
      conf_set_int_int(cfg.get(), CONF_colours, currind, rgba);
    }
  }
}

void GuiSettingsWindow::on_l_colour_currentItemChanged(QListWidgetItem *current,
                                                       QListWidgetItem *previous) {
  int prev = -1, curr = -1;
  if (previous) {
    prev = ui->l_colour->row(previous);
  }
  if (current) {
    curr = ui->l_colour->row(current);
  }
  qDebug() << prev << curr;
  if (prev >= 0 && prev < NCFGCOLOURS) {
    int r = ui->le_colour_r->text().toInt();
    int g = ui->le_colour_g->text().toInt();
    int b = ui->le_colour_b->text().toInt();
    int rgba = qRgba(r, g, b, 0xFF);
    conf_set_int_int(cfg.get(), CONF_colours, prev, rgba);
  }
  if (curr >= 0 && curr < NCFGCOLOURS) {
    QRgb rgba = conf_get_int_int(cfg.get(), CONF_colours, curr);
    QColor col = QColor::fromRgba(rgba);
    ui->le_colour_r->setText(QString::number(col.red()));
    ui->le_colour_g->setText(QString::number(col.green()));
    ui->le_colour_b->setText(QString::number(col.blue()));
  }
}

void GuiSettingsWindow::on_b_sess_copy_clicked() {
  QTreeWidgetItem *item = ui->l_saved_sess->currentItem();
  QString fullPathName;
  if (!item) return;
  QString fullname;
  QTreeWidgetItem *parent = item->parent();
  if (!parent) {
    parent = ui->l_saved_sess->invisibleRootItem();
    fullPathName = item->text(0);
  } else {
    fullname = parent->data(0, QUTTY_ROLE_FULL_SESSNAME).toString() + QUTTY_SESSION_NAME_SPLIT;
    fullPathName = fullname + item->text(0);
  }
  QTreeWidgetItem *newitem = new QTreeWidgetItem(0);
  QString foldername = item->text(0);
  for (int i = 1;; i++) {
    bool isunique = true;
    for (int j = 0; j < parent->childCount(); j++) {
      if (parent->child(j)->text(0) == foldername) {
        isunique = false;
        foldername = item->text(0) + tr(" (") + QString::number(i) + tr(")");
        break;
      }
    }
    if (isunique) {
      break;
    }
  }

  fullname += foldername;
  newitem->setText(0, foldername);
  newitem->setData(0, QUTTY_ROLE_FULL_SESSNAME, fullname);
  parent->insertChild(parent->indexOfChild(item) + 1, newitem);
  QtConfig::Pointer cfg = QtConfig::copy(qutty_config.config_list[fullPathName].get());
  conf_set_str(cfg.get(), CONF_config_name, fullname.toUtf8());
  qutty_config.config_list[fullname] = std::move(cfg);

  pending_session_changes = true;
}

static bool conf_del_str_all(Conf *cfg, config_primary_key key) {
  std::vector<std::string> subkeys;
  char *subkey = nullptr;
  while (conf_get_str_strs(cfg, key, subkey, &subkey)) subkeys.push_back(subkey);
  for (const std::string &sk : subkeys) conf_del_str_str(cfg, key, sk.c_str());
  return !subkeys.empty();
}

void chkUnsupportedConfigs(Conf *cfg) {
  QString opt_unsupp;

  if (confKeyExists(cfg, CONF_try_gssapi_auth)) {
    conf_set_int(cfg, CONF_try_gssapi_auth, 0);
  }

  if (conf_del_str_all(cfg, CONF_portfwd)) {
    opt_unsupp += " * SSH Tunnels/port forwarding\n";
  }

  if (confKeyExists(cfg, CONF_tryagent)) {
    conf_set_int(cfg, CONF_tryagent, 0);
  }

  conf_del_str_all(cfg, CONF_ttymodes);

  if (opt_unsupp.length() > 0)
    QMessageBox::warning(
        NULL, QObject::tr("Qutty Configuration"),
        QObject::tr("Following options are not yet supported in QuTTY.\n\n%1").arg(opt_unsupp));
}

void GuiSettingsWindow::on_btn_sessionlog_filename_browse_clicked() {
  ui->le_sessionlog_filename->setText(QFileDialog::getSaveFileName(
      this, tr("Select session log filename"), "qutty", tr("log files (*.log)")));
}
