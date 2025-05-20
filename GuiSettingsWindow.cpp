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
#include <QListWidgetItem>
#include <QMessageBox>
#include <QRadioButton>
#include <QString>
#include <QTableWidgetItem>
#include <QTreeWidgetItem>
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
  ui->treeWidget->topLevelItem(3)->child(4)->child(1)->setData(0, Qt::UserRole, GUI_PAGE_CIPHER);
  ui->treeWidget->topLevelItem(3)->child(4)->child(2)->setData(0, Qt::UserRole, GUI_PAGE_AUTH);
  ui->treeWidget->topLevelItem(3)->child(4)->child(2)->child(0)->setData(0, Qt::UserRole,
                                                                         GUI_PAGE_GSSAPI);
  ui->treeWidget->topLevelItem(3)->child(4)->child(3)->setData(0, Qt::UserRole, GUI_PAGE_TTY);
  ui->treeWidget->topLevelItem(3)->child(4)->child(4)->setData(0, Qt::UserRole, GUI_PAGE_X11);
  ui->treeWidget->topLevelItem(3)->child(4)->child(5)->setData(0, Qt::UserRole, GUI_PAGE_TUNNELS);
  ui->treeWidget->topLevelItem(3)->child(4)->child(6)->setData(0, Qt::UserRole, GUI_PAGE_BUGS);
  ui->treeWidget->topLevelItem(3)->child(5)->setData(0, Qt::UserRole, GUI_PAGE_SERIAL);

  // expand all 1st level items
  ui->treeWidget->expandToDepth(0);

  ui->rb_contype_raw->setVisible(false);
  ui->rb_contype_rlogin->setVisible(false);
  ui->rb_contype_serial->setVisible(false);

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

enum class UI {
  None = 0,
  Optional = 1,
};

Q_DECLARE_FLAGS(UIFlags, UI)
Q_DECLARE_OPERATORS_FOR_FLAGS(UIFlags)

#define UI_MAPPING(X)                                                          \
  /* control name, keyword, value (opt) */                                     \
                                                                               \
  /* Session */                                                                \
  X("le_hostname", host)                                                       \
  X("le_port", port)                                                           \
  X("rb_contype_raw", protocol, PROT_RAW)                                      \
  X("rb_contype_telnet", protocol, PROT_TELNET)                                \
  X("rb_contype_rlogin", protocol, PROT_RLOGIN)                                \
  X("rb_contype_ssh", protocol, PROT_SSH)                                      \
  X("rb_contype_serial", protocol, PROT_SERIAL)                                \
  X("rb_exit_always", close_on_exit, FORCE_ON)                                 \
  X("rb_exit_clean", close_on_exit, FORCE_OFF)                                 \
  X("rb_exit_never", close_on_exit, AUTO)                                      \
  /* Session Logging v*/                                                       \
  X("rb_sessionlog_none", logtype, LGTYP_NONE)                                 \
  X("rb_sessionlog_printout", logtype, LGTYP_ASCII)                            \
  X("rb_sessionlog_alloutput", logtype, LGTYP_DEBUG)                           \
  X("rb_sessionlog_sshpacket", logtype, LGTYP_PACKETS)                         \
  X("rb_sessionlog_rawdata", logtype, LGTYP_SSHRAW)                            \
  X("le_sessionlog_filename", logfilename)                                     \
  X("rb_sessionlog_overwrite", logxfovr, LGXF_OVR)                             \
  X("rb_sessionlog_append", logxfovr, LGXF_APN)                                \
  X("rb_sessionlog_askuser", logxfovr, LGXF_ASK)                               \
  X("chb_sessionlog_flush", logflush)                                          \
  X("chb_sessionlog_omitpasswd", logomitpass)                                  \
  X("chb_sessionlog_omitdata", logomitdata)                                    \
  /* Terminal Emulation */                                                     \
  X("chb_terminaloption_autowrap", wrap_mode)                                  \
  X("chb_terminaloption_decorigin", dec_om)                                    \
  X("chb_terminaloption_lf", lfhascr)                                          \
  X("chb_terminaloption_lf", crhaslf)                                          \
  X("chb_terminaloption_bgcolor", bce)                                         \
  X("chb_terminaloption_blinktext", blinktext)                                 \
  X("le_termopt_ansback", answerback)                                          \
  X("rb_termopt_echoauto", localecho, AUTO)                                    \
  X("rb_termopt_echoon", localecho, FORCE_ON)                                  \
  X("rb_termopt_echooff", localecho, FORCE_OFF)                                \
  X("rb_termopt_editauto", localedit, AUTO)                                    \
  X("rb_termopt_editon", localedit, FORCE_ON)                                  \
  X("rb_termopt_editoff", localedit, FORCE_OFF)                                \
  X("cb_termopt_printeroutput", printer)                                       \
  /* Keyboard */                                                               \
  X("rb_backspacekey_ctrlh", bksp_is_delete, true)                             \
  X("rb_backspace_ctrl127", bksp_is_delete, false)                             \
  X("rb_homeendkeys_rxvt", rxvt_homeend, true)                                 \
  X("rb_homeendkeys_std", rxvt_homeend, false)                                 \
  X("rb_fnkeys_esc", funky_type, FUNKY_TILDE)                                  \
  X("rb_fnkeys_linux", funky_type, FUNKY_LINUX)                                \
  X("rb_fnkeys_xtermr6", funky_type, FUNKY_XTERM)                              \
  X("rb_fnkeys_vt400", funky_type, FUNKY_VT400)                                \
  X("rb_fnkeys_vt100", funky_type, FUNKY_VT100P)                               \
  X("rb_fnkeys_sco", funky_type, FUNKY_SCO)                                    \
  X("rb_inicursorkeys_normal", app_cursor, false)                              \
  X("rb_inicursorkeys_app", app_cursor, true)                                  \
  X("rb_ininumkeys_normal", nethack_keypad, 0)                                 \
  X("rb_ininumkeys_app", nethack_keypad, 1)                                    \
  X("rb_ininumkeys_nethack", nethack_keypad, 2)                                \
  X("chb_altgrkey", compose_key)                                               \
  X("chb_ctrl_alt", ctrlaltkeys)                                               \
  /* Bell */                                                                   \
  X("rb_bellaction_none", beep, BELL_DISABLED)                                 \
  X("rb_bellaction_alertsound", beep, BELL_DEFAULT)                            \
  X("rb_bellaction_visual", beep, BELL_VISUAL)                                 \
  X("rb_bellaction_beep", beep, BELL_PCSPEAKER)                                \
  X("rb_bellstyle_playcustomsound", beep, BELL_WAVEFILE)                       \
  X("le_bell_wavefile", bell_wavefile)                                         \
  X("rb_belltaskbar_disable", beep_ind, B_IND_DISABLED)                        \
  X("rb_belltaskbar_flash", beep_ind, B_IND_FLASH)                             \
  X("rb_belltaskbar_steady", beep_ind, B_IND_STEADY)                           \
  X("chb_bellcontrol_whenoverused", bellovl)                                   \
  X("le_bellcontrol_overuseno", bellovl_n)                                     \
  X("le_bellcontrol_overusesec", bellovl_t)                                    \
  X("le_bellcontrol_silencereq_secs", bellovl_s)                               \
  /* Advanced Terminal Features */                                             \
  X("chb_no_applic_c", no_applic_c)                                            \
  X("chb_no_applic_k", no_applic_k)                                            \
  X("chb_no_mouse_rep", no_mouse_rep)                                          \
  X("chb_no_remote_resize", no_remote_resize)                                  \
  X("chb_no_alt_screen", no_alt_screen)                                        \
  X("chb_no_remote_wintitle", no_remote_wintitle)                              \
  X("rb_featqtitle_none", remote_qtitle_action, TITLE_NONE)                    \
  X("rb_featqtitle_empstring", remote_qtitle_action, TITLE_EMPTY)              \
  X("rb_featqtitle_wndtitle", remote_qtitle_action, TITLE_REAL)                \
  X("chb_no_dbackspace", no_dbackspace)                                        \
  X("chb_no_remote_charset", no_remote_charset)                                \
  X("chb_no_arabic", arabicshaping)                                            \
  X("chb_no_bidi", bidi)                                                       \
  /* Window */                                                                 \
  X("le_window_column", width)                                                 \
  X("le_window_row", height)                                                   \
  X("rb_wndresz_rowcolno", resize_action, RESIZE_TERM)                         \
  X("rb_wndresz_fontsize", resize_action, RESIZE_FONT)                         \
  X("rb_wndresz_onlywhenmax", resize_action, RESIZE_EITHER)                    \
  X("rb_wndresz_forbid", resize_action, RESIZE_DISABLED)                       \
  X("le_wndscroll_lines", savelines)                                           \
  X("chb_wndscroll_display", scrollbar)                                        \
  X("chb_wndscroll_fullscreen", scrollbar_in_fullscreen)                       \
  X("chb_wndscroll_resetkeypress", scroll_on_key)                              \
  X("chb_wndscroll_resetdisply", scroll_on_disp)                               \
  X("chb_wndscroll_pusherasedtext", erase_to_scrollback)                       \
  /* Window Appearance */                                                      \
  X("rb_curappear_block", cursor_type, 0)                                      \
  X("rb_curappear_underline", cursor_type, 1)                                  \
  X("rb_curappear_vertline", cursor_type, 2)                                   \
  X("chb_curblink", blink_cur)                                                 \
  /* X("chb_fontsel_varpitch", 0) */                                           \
  X("rb_fontappear_antialiase", font_quality, FQ_ANTIALIASED)                  \
  X("rb_fontappear_nonantialiase", font_quality, FQ_NONANTIALIASED)            \
  X("rb_fontappear_clear", font_quality, FQ_CLEARTYPE)                         \
  X("rb_fontappear_default", font_quality, FQ_DEFAULT)                         \
  X("chb_hide_mouseptr", hide_mouseptr)                                        \
  X("le_borderappear_gap", window_border)                                      \
  X("chb_sunken_edge", sunken_edge)                                            \
  /* Window Behaviour */                                                       \
  X("le_wintitle", wintitle)                                                   \
  X("chb_separate_titles", win_name_always)                                    \
  X("chb_behaviour_warn", warn_on_close)                                       \
  X("chb_behaviour_altf4", alt_f4)                                             \
  X("chb_behaviour_alt_space", alt_space)                                      \
  X("chb_behaviour_altalone", alt_only)                                        \
  X("chb_behaviour_top", alwaysontop)                                          \
  X("chb_behaviour_alt_enter", fullscreenonaltenter)                           \
  /* Character Set Translation */                                              \
  X("cb_codepage", line_codepage)                                              \
  X("chb_cjk_ambig_wide", cjk_ambig_wide)                                      \
  X("chb_xlat_capslockcyr", xlat_capslockcyr)                                  \
  X("rb_translation_useunicode", vtmode, VT_UNICODE)                           \
  X("rb_translation_poorman", vtmode, VT_POORMAN)                              \
  X("rb_translation_xwindows", vtmode, VT_XWINDOWS)                            \
  X("rb_translation_ansi_oem", vtmode, VT_OEMANSI)                             \
  X("rb_translation_oem", vtmode, VT_OEMONLY)                                  \
  X("chb_translation_copy_paste", rawcnp)                                      \
  /* Selection */                                                              \
  X("rb_cpmouseaction_windows", mouse_is_xterm, 0)                             \
  X("rb_cpmouseaction_compromise", mouse_is_xterm, 2) /* TODO chk */           \
  X("rb_cpmouseaction_xterm", mouse_is_xterm, 1)                               \
  X("chb_cpmouseaction_shift", mouse_override)                                 \
  X("rb_cpmouseaction_defaultnormal", rect_select, 0)                          \
  X("rb_cpmouseaction_defaultrect", rect_select, 1)                            \
  X("l_char_classes", wordness)                                                \
  X("chb_rtf_paste", rtf_paste)                                                \
  /* Colour */                                                                 \
  X("chb_coloursoption_ansi", ansi_colour)                                     \
  X("chb_coloursoption_xterm", xterm_256_colour)                               \
  X("rb_bold_font", bold_style, 1)                                             \
  X("rb_bold_colour", bold_style, 2)                                           \
  X("rb_bold_both", bold_style, 3)                                             \
  X("chb_colouroption_palette", try_palette)                                   \
  X("chb_colouroption_usesystem", system_colour)                               \
  X("l_colour", colours)                                                       \
  /* Connection  */                                                            \
  X("le_ping_interval", ping_interval)                                         \
  X("chb_tcp_nodelay", tcp_nodelay)                                            \
  X("chb_tcp_keepalive", tcp_keepalives)                                       \
  X("rb_connectprotocol_auto", addressfamily, ADDRTYPE_UNSPEC)                 \
  X("rb_connectprotocol_ipv4", addressfamily, ADDRTYPE_IPV4)                   \
  X("rb_connectprotocol_ipv6", addressfamily, ADDRTYPE_IPV6)                   \
  X("le_loghost", loghost)                                                     \
  /* Connection Data  */                                                       \
  X("le_datausername", username)                                               \
  X("rb_datausername_systemsuse", username_from_env, true)                     \
  X("rb_datausername_prompt", username_from_env, false)                        \
  X("le_termtype", termtype)                                                   \
  X("le_termspeed", termspeed)                                                 \
  X("l_env_vars", environmt)                                                   \
  /* Proxy */                                                                  \
  X("rb_proxytype_none", proxy_type, PROXY_NONE)                               \
  X("rb_proxy_socks4", proxy_type, PROXY_SOCKS4)                               \
  X("rb_proxytype_socks5", proxy_type, PROXY_SOCKS5)                           \
  X("rb_proxytype_http", proxy_type, PROXY_HTTP)                               \
  X("rb_proxytype_telnet", proxy_type, PROXY_TELNET)                           \
  X("rb_proxytype_local", proxy_type, PROXY_CMD)                               \
  X("le_proxy_host", proxy_host)                                               \
  X("le_proxy_port", proxy_port)                                               \
  X("le_proxy_exclude_list", proxy_exclude_list)                               \
  X("chb_even_proxy_localhost", even_proxy_localhost)                          \
  X("rb_proxydns_no", proxy_dns, FORCE_OFF)                                    \
  X("rb_proxydns_auto", proxy_dns, AUTO)                                       \
  X("rd_proxydns_yes", proxy_dns, FORCE_ON)                                    \
  X("le_proxy_username", proxy_username)                                       \
  X("le_proxy_password", proxy_password)                                       \
  X("le_proxy_telnet_command", proxy_telnet_command)                           \
  /* Telnet */                                                                 \
  X("rb_telnetprotocol_bsd", rfc_environ, false)                               \
  X("rb_telnetprotocol_rfc1408", rfc_environ, true)                            \
  X("rb_telnetnegotiate_passsive", passive_telnet, true)                       \
  X("rb_telnetnegotiate_active", passive_telnet, false)                        \
  X("chb_telnet_keyboard", telnet_keyboard)                                    \
  X("chb_telnet_returnkey", telnet_newline)                                    \
  /* Rlogin */                                                                 \
  X("le_local_username", localusername)                                        \
  /* SSH Connections */                                                        \
  X("le_remote_cmd", remote_cmd)                                               \
  X("chb_ssh_no_shell", ssh_no_shell)                                          \
  X("chb_compression", compression)                                            \
  X("rb_sshprotocol_1only", sshprot, 0)                                        \
  X("rb_sshprotocol_1", sshprot, 1)                                            \
  X("rb_sshprotocol_2", sshprot, 2)                                            \
  X("rb_sshprotocol_2only", sshprot, 3)                                        \
  X("cb_ssh_connection_sharing", ssh_connection_sharing)                       \
  X("cb_ssh_connection_sharing_upstream", ssh_connection_sharing_upstream)     \
  X("cb_ssh_connection_sharing_downstream", ssh_connection_sharing_downstream) \
  /* SSH Key Exchange */                                                       \
  X("l_ssh_kexlist", ssh_kexlist)                                              \
  X("le_ssh_rekey_time", ssh_rekey_time)                                       \
  X("le_ssh_rekey_time", ssh_rekey_data)                                       \
  X("l_ssh_manual_hostkeys", ssh_manual_hostkeys)                              \
  /* SSH Cipher */                                                             \
  X("l_ssh_cipherlist", ssh_cipherlist)                                        \
  X("chb_ssh2_des_cbc", ssh2_des_cbc)                                          \
  /* SSH Authentication */                                                     \
  X("chb_ssh_no_userauth", ssh_no_userauth)                                    \
  X("chb_ssh_show_banner", ssh_show_banner)                                    \
  X("chb_ssh_tryagent", tryagent)                                              \
  X("chb_ssh_try_tis_auth", try_tis_auth)                                      \
  X("chb_ssh_try_ki_auth", try_ki_auth)                                        \
  X("chb_ssh_agentfwd", agentfwd)                                              \
  X("chb_ssh_change_username", change_username)                                \
  X("le_ssh_auth_keyfile", keyfile)                                            \
  /* GSSAPI Authentication */                                                  \
  X("chb_gssapi_authen", try_gssapi_auth)                                      \
  X("chb_gssapi_credential", gssapifwd)                                        \
  X("l_ssh_gsslist", ssh_gsslist)                                              \
  X("le_ssh_gss_custom", ssh_gss_custom, UI::Optional)                         \
  /* Remote Terminal */                                                        \
  X("chb_nopty", nopty)                                                        \
  X("l_ttymodes", ttymodes)                                                    \
  /* X11 */                                                                    \
  X("chb_x11_forward", x11_forward)                                            \
  X("le_x11_display", x11_display)                                             \
  X("rb_x11remote_mit", x11_auth, 0)                                           \
  X("rb_x11remote_xdm", x11_auth, 1)                                           \
  X("le_xauthfile", xauthfile)                                                 \
  /* SSH Port Forwarding */                                                    \
  X("chb_lport_acceptall", lport_acceptall)                                    \
  X("chb_rport_acceptall", rport_acceptall)                                    \
  X("l_portfwd", portfwd)                                                      \
  /* SSH Server Bug Workarounds */                                             \
  X("cb_sshbug_ignore1", sshbug_ignore1)                                       \
  X("cb_sshbug_plainpw1", sshbug_plainpw1)                                     \
  X("cb_sshbug_rsa1", sshbug_rsa1)                                             \
  X("cb_sshbug_ignore2", sshbug_ignore2)                                       \
  X("cb_sshbug_winadj", sshbug_winadj)                                         \
  X("cb_sshbug_hmac2", sshbug_hmac2)                                           \
  X("cb_sshbug_derivekey2", sshbug_derivekey2)                                 \
  X("cb_sshbug_rsapad2", sshbug_rsapad2)                                       \
  X("cb_sshbug_pksessid2", sshbug_pksessid2)                                   \
  X("cb_sshbug_rekey2", sshbug_rekey2)                                         \
  X("cb_sshbug_maxpkt2", sshbug_maxpkt2)                                       \
  X("cb_sshbug_chanreq", sshbug_chanreq)                                       \
  X("cb_sshbug_oldgex2", sshbug_oldgex2)                                       \
  /* Serial Port  */                                                           \
  X("le_serial_line", serline)                                                 \
  X("le_config_speed", serspeed)                                               \
  X("le_config_data", serdatabits)                                             \
  X("le_config_stop", serstopbits)                                             \
  X("cb_config_parity", serparity)                                             \
  X("cb_config_flow", serflow)                                                 \
  /* */                                                                        \
  X("rb_ininumkeys_app", app_keypad, true)

/*
 * These options are yet to be incorporated into the UI.
 * They are here to ensure they are valid CONF_ keys.
 */
#define UI_MAPPING_TODO(X)                 \
  /* control name, keyword, value (opt) */ \
  /* SSH options */                        \
  X("", remote_cmd2)                       \
  X("", ssh_subsys)                        \
  X("", ssh_subsys2)                       \
  X("", ssh_nc_host)                       \
  X("", ssh_nc_port)                       \
  /* translations */                       \
  X("", utf8_override)                     \
  /* SSH Bug Compatibility Modes */        \
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
  X("", winclass)

#define CONF_VALIDATOR(name, key, ...) key = CONF_##key,
enum class UI_MAPPING_TODO_VALID { UI_MAPPING_TODO(CONF_VALIDATOR) };

#define UI_TO_KEY_ENTRY(ui, key, ...) {ui, {CONF_##key __VA_OPT__(, ) __VA_ARGS__}},
struct KeyValue {
  config_primary_key key;
  int value = -1;
  bool is_bool = false;
  UIFlags options = UI::None;

  KeyValue(config_primary_key key, bool value, UIFlags opt = UI::None)
      : key(key), value(int(value)), is_bool(true), options(opt) {}
  KeyValue(config_primary_key key, int value, UIFlags opt = UI::None)
      : key(key), value(value), options(opt) {}
  KeyValue(config_primary_key key, UIFlags opt = UI::None) : key(key), options(opt) {}
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
    UIFlags opt = kv.options;

    if (opt & UI::Optional && !confKeyExists(cfg.get(), key)) continue;
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
  ui->chb_sessionlog_flush->setChecked(true);

  /* window options */
  FontSpec &font = *conf_get_fontspec(cfg.get(), CONF_font);
  ui->lbl_fontsel->setText(
      QString("%1, %2%3-point")
          .arg(font.name, font.isbold ? "Bold, " : "", QString::number(font.height)));
  ind = ui->cb_codepage->findText(conf_get_str(cfg.get(), CONF_line_codepage));
  if (ind == -1) ind = 0;
  ui->cb_codepage->setCurrentIndex(ind);

  initWordness();
  initColours();
  initEnvVars();
  initCipherList();
  initKexList();
  initTTYModes();
  initPortFwds();
#ifndef NO_GSSAPI
  initGSSList();
#endif
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

static Qt::ItemFlags const CONST_FLAGS =
    Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable;

void GuiSettingsWindow::initWordness() {
  QTableWidget *table = ui->l_char_classes;
  table->setRowCount(256);
  table->setColumnCount(3);
  table->horizontalHeader()->hide();
  table->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
  for (int ch = 0; ch < 256; ch++) {
    int wordness = conf_get_int_int(cfg.get(), CONF_wordness, ch);
    auto *col0 = new QTableWidgetItem(QString::asprintf("(0x%02X)", ch));
    auto *col1 = new QTableWidgetItem((ch >= 32 && ch <= 127) ? QString(QChar(ch)) : QString());
    auto *col2 = new QTableWidgetItem(QString::number(wordness));
    col0->setFlags(CONST_FLAGS);
    col1->setFlags(CONST_FLAGS);
    table->setItem(ch, 0, col0);
    table->setItem(ch, 1, col1);
    table->setItem(ch, 2, col2);
  }
}

void GuiSettingsWindow::on_l_char_classes_currentItemChanged(QTableWidgetItem *item) {
  QTableWidget *table = ui->l_char_classes;
  ui->le_char_class->setText(table->item(item->row(), 2)->text());
}

void GuiSettingsWindow::on_btn_char_class_set_clicked() {
  bool ok;
  int cclass = ui->le_char_class->text().toInt(&ok);
  if (ok) {
    QTableWidget *table = ui->l_char_classes;
    int ch = table->currentRow();
    table->item(ch, 2)->setText(QString::number(cclass));
    conf_set_int_int(cfg.get(), CONF_wordness, ch, cclass);
  }
}

static void setEnvRow(QTableWidget *table, int row, const QString &text0, const QString &text1) {
  auto *col0 = new QTableWidgetItem(text0);
  auto *col1 = new QTableWidgetItem(text1);
  col0->setFlags(CONST_FLAGS);
  table->setItem(row, 0, col0);
  table->setItem(row, 1, col1);
}

static int conf_get_str_count(Conf *conf, config_primary_key key) {
  int rows = 0;
  char *subkey = nullptr;
  while (conf_get_str_strs(conf, key, subkey, &subkey)) rows++;
  return rows;
}

void GuiSettingsWindow::initEnvVars() {
  QTableWidget *table = ui->l_env_vars;
  int rows = conf_get_str_count(cfg.get(), CONF_environmt);
  table->setColumnCount(2);
  table->setRowCount(rows);
  table->verticalHeader()->hide();
  table->horizontalHeader()->hide();
  table->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);

  char *value = nullptr;
  char *subkey = nullptr;
  int row = 0;
  while ((value = conf_get_str_strs(cfg.get(), CONF_environmt, subkey, &subkey))) {
    setEnvRow(table, row, subkey, value);
    row++;
  }
}

void GuiSettingsWindow::on_pb_env_add_clicked() {
  QTableWidget *table = ui->l_env_vars;
  auto name = ui->le_env_var->text();
  auto value = ui->le_env_value->text();
  if (name.isEmpty() || value.isEmpty()) return;
  auto where = table->findItems(name, Qt::MatchExactly);
  int row = table->rowCount();
  bool newRow = true;
  if (!where.isEmpty()) {
    for (const auto *item : std::as_const(where)) {
      if (item->column() == 0) {
        row = item->row();
        newRow = false;
        break;
      }
    }
  }
  if (newRow) table->setRowCount(table->rowCount() + 1);
  setEnvRow(table, row, name, value);
  ui->le_env_var->clear();
  ui->le_env_value->clear();
  conf_set_str_str(cfg.get(), CONF_environmt, name.toUtf8(), value.toUtf8());
}

void GuiSettingsWindow::on_pb_env_remove_clicked() {
  QTableWidget *table = ui->l_env_vars;
  auto *item = table->currentItem();
  if (!item) return;
  int row = item->row();
  auto name = item->text();
  table->removeRow(row);
  conf_del_str_str(cfg.get(), CONF_environmt, name.toUtf8());
}

static bool listMoveUp(QListWidget *list) {
  int row = list->currentRow();
  if (row < 1) return false;
  auto *item = list->takeItem(row);
  list->insertItem(row - 1, item);
  list->setCurrentRow(row - 1);
  return true;
}

static bool listMoveDown(QListWidget *list) {
  int row = list->currentRow();
  if (row < 0 || row >= list->count() - 1) return false;
  auto *item = list->takeItem(row);
  list->insertItem(row + 1, item);
  list->setCurrentRow(row + 1);
  return true;
}

struct DataItem {
  const char *text;
  int data;
};

struct DataList {
  config_primary_key key;
  int count;
  const DataItem *items;

  constexpr DataList(config_primary_key key, int count, const DataItem items[])
      : key(key), count(count), items(items) {}

  void initList(QListWidget *list, Conf *conf) const {
    list->clear();
    std::vector<QListWidgetItem *> listItems;
    listItems.resize(count);
    for (int i = 0; i < count; i++) {
      auto *item = new QListWidgetItem(items[i].text);
      item->setData(Qt::UserRole, items[i].data);
      listItems[items[i].data] = item;
    }
    for (int i = 0; i < count; i++) {
      int const data = conf_get_int_int(conf, key, i);
      for (int j = 0; j < count; j++) {
        auto &item = items[j];
        if (item.data == data) {
          list->addItem(listItems[item.data]);
          break;
        }
      }
    }
  }

  void updateConfig(QListWidget *list, Conf *conf) const {
    for (int i = 0; i < count; i++) {
      auto *item = list->item(i);
      int value = item->data(Qt::UserRole).toInt();
      conf_set_int_int(conf, key, i, value);
    }
  }
};

// From putty-0.63/config.c
static const DataItem ciphers[] = {{"3DES", CIPHER_3DES},
                                   {"Blowfish", CIPHER_BLOWFISH},
                                   {"DES", CIPHER_DES},
                                   {"AES (SSH-2 only)", CIPHER_AES},
                                   {"Arcfour (SSH-2 only)", CIPHER_ARCFOUR},
                                   {"-- warn below here --", CIPHER_WARN}};

static const DataList cipherDataList(CONF_ssh_cipherlist, CIPHER_MAX, ciphers);

void GuiSettingsWindow::initCipherList() {
  cipherDataList.initList(ui->l_ssh_cipherlist, cfg.get());
}

void GuiSettingsWindow::on_pb_ssh_cipher_up_clicked() {
  if (listMoveUp(ui->l_ssh_cipherlist))
    cipherDataList.updateConfig(ui->l_ssh_cipherlist, cfg.get());
}

void GuiSettingsWindow::on_pb_ssh_cipher_down_clicked() {
  if (listMoveDown(ui->l_ssh_cipherlist))
    cipherDataList.updateConfig(ui->l_ssh_cipherlist, cfg.get());
}

// From putty-0.63/config.c
static const DataItem kexes[] = {{"Diffie-Hellman group 1", KEX_DHGROUP1},
                                 {"Diffie-Hellman group 14", KEX_DHGROUP14},
                                 {"Diffie-Hellman group exchange", KEX_DHGEX},
                                 {"RSA-based key exchange", KEX_RSA},
                                 {"-- warn below here --", KEX_WARN}};

static const DataList kexDataList(CONF_ssh_kexlist, KEX_MAX, kexes);

void GuiSettingsWindow::initKexList() { kexDataList.initList(ui->l_ssh_kexlist, cfg.get()); }

void GuiSettingsWindow::on_pb_ssh_kex_up_clicked() {
  if (listMoveUp(ui->l_ssh_kexlist)) kexDataList.updateConfig(ui->l_ssh_kexlist, cfg.get());
}

void GuiSettingsWindow::on_pb_ssh_kex_down_clicked() {
  if (listMoveDown(ui->l_ssh_kexlist)) kexDataList.updateConfig(ui->l_ssh_kexlist, cfg.get());
}

enum ModeType { TTY_OP_CHAR, TTY_OP_BOOL };

struct TTYModeDecl {
  const char *const name;
  int opcode;
  ModeType type;
};

// from putty-0.63/ssh.c
static const TTYModeDecl ssh_ttymodes[] = {
    /* "V" prefix discarded for special characters relative to SSH specs */
    {"INTR", 1, TTY_OP_CHAR},    {"QUIT", 2, TTY_OP_CHAR},     {"ERASE", 3, TTY_OP_CHAR},
    {"KILL", 4, TTY_OP_CHAR},    {"EOF", 5, TTY_OP_CHAR},      {"EOL", 6, TTY_OP_CHAR},
    {"EOL2", 7, TTY_OP_CHAR},    {"START", 8, TTY_OP_CHAR},    {"STOP", 9, TTY_OP_CHAR},
    {"SUSP", 10, TTY_OP_CHAR},   {"DSUSP", 11, TTY_OP_CHAR},   {"REPRINT", 12, TTY_OP_CHAR},
    {"WERASE", 13, TTY_OP_CHAR}, {"LNEXT", 14, TTY_OP_CHAR},   {"FLUSH", 15, TTY_OP_CHAR},
    {"SWTCH", 16, TTY_OP_CHAR},  {"STATUS", 17, TTY_OP_CHAR},  {"DISCARD", 18, TTY_OP_CHAR},
    {"IGNPAR", 30, TTY_OP_BOOL}, {"PARMRK", 31, TTY_OP_BOOL},  {"INPCK", 32, TTY_OP_BOOL},
    {"ISTRIP", 33, TTY_OP_BOOL}, {"INLCR", 34, TTY_OP_BOOL},   {"IGNCR", 35, TTY_OP_BOOL},
    {"ICRNL", 36, TTY_OP_BOOL},  {"IUCLC", 37, TTY_OP_BOOL},   {"IXON", 38, TTY_OP_BOOL},
    {"IXANY", 39, TTY_OP_BOOL},  {"IXOFF", 40, TTY_OP_BOOL},   {"IMAXBEL", 41, TTY_OP_BOOL},
    {"ISIG", 50, TTY_OP_BOOL},   {"ICANON", 51, TTY_OP_BOOL},  {"XCASE", 52, TTY_OP_BOOL},
    {"ECHO", 53, TTY_OP_BOOL},   {"ECHOE", 54, TTY_OP_BOOL},   {"ECHOK", 55, TTY_OP_BOOL},
    {"ECHONL", 56, TTY_OP_BOOL}, {"NOFLSH", 57, TTY_OP_BOOL},  {"TOSTOP", 58, TTY_OP_BOOL},
    {"IEXTEN", 59, TTY_OP_BOOL}, {"ECHOCTL", 60, TTY_OP_BOOL}, {"ECHOKE", 61, TTY_OP_BOOL},
    {"PENDIN", 62, TTY_OP_BOOL}, {"OPOST", 70, TTY_OP_BOOL},   {"OLCUC", 71, TTY_OP_BOOL},
    {"ONLCR", 72, TTY_OP_BOOL},  {"OCRNL", 73, TTY_OP_BOOL},   {"ONOCR", 74, TTY_OP_BOOL},
    {"ONLRET", 75, TTY_OP_BOOL}, {"CS7", 90, TTY_OP_BOOL},     {"CS8", 91, TTY_OP_BOOL},
    {"PARENB", 92, TTY_OP_BOOL}, {"PARODD", 93, TTY_OP_BOOL}};

static const TTYModeDecl *getModeDecl(const char *name) {
  for (auto const &mode : ssh_ttymodes)
    if (strcmp(mode.name, name) == 0) return &mode;
  return nullptr;
}

static QString formatTTYMode(const QString &value) {
  if (value == "A") return "(auto)";
  if (value.startsWith("V")) return value.mid(1);
  return "(auto?)";  // should not happen
}

static void setTableRow(QTableWidget *table, int row, const char *left, const char *right) {
  auto *col0 = new QTableWidgetItem(left);
  auto *col1 = new QTableWidgetItem(right);
  col0->setFlags(CONST_FLAGS);
  col1->setFlags(CONST_FLAGS);
  table->setItem(row, 0, col0);
  table->setItem(row, 1, col1);
}

static QByteArray removeCurrentRow(QTableWidget *table) {
  int row = table->currentRow();
  if (row < 0) return {};
  auto left = table->item(row, 0)->text().toLatin1();
  table->removeRow(row);
  return left;
}

void GuiSettingsWindow::initTTYModes() {
  ui->cb_ttymodes->clear();
  for (auto const &m : ssh_ttymodes) ui->cb_ttymodes->addItem(m.name);

  int rows = conf_get_str_count(cfg.get(), CONF_ttymodes);
  QTableWidget *table = ui->l_ttymodes;
  table->clear();
  table->setRowCount(rows);
  table->setColumnCount(2);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->verticalHeader()->hide();
  table->horizontalHeader()->hide();

  const char *value = nullptr;
  char *subkey = nullptr;
  int i = 0;
  while ((value = conf_get_str_strs(cfg.get(), CONF_ttymodes, subkey, &subkey))) {
    auto text = formatTTYMode(value);
    setTableRow(table, i, subkey, text.toLatin1());
    i++;
  }
  table->sortItems(0);
}

void GuiSettingsWindow::on_pb_ttymodes_remove_clicked() {
  QByteArray name = removeCurrentRow(ui->l_ttymodes);
  if (!name.isEmpty()) conf_del_str_str(cfg.get(), CONF_ttymodes, name);
}

void GuiSettingsWindow::on_pb_ttymodes_add_clicked() {
  QString const name = ui->cb_ttymodes->currentText();
  QByteArray const bname = name.toLatin1();
  QString value = "A";
  if (ui->rb_terminalvalue_this->isChecked()) value = "V" + ui->le_ttymode->text();
  QByteArray bvalue = value.toLatin1();

  conf_set_str_str(cfg.get(), CONF_ttymodes, bname, bvalue);

  QTableWidget *table = ui->l_ttymodes;
  auto const items = table->findItems(name, Qt::MatchExactly);
  int row = table->rowCount();
  if (items.isEmpty()) {
    table->setRowCount(row + 1);
  } else {
    for (auto *item : items) {
      if (item->column() == 0) {
        row = item->row();
        break;
      }
    }
  }
  value = formatTTYMode(value);
  bvalue = value.toLatin1();
  setTableRow(table, row, bname, bvalue);
  table->sortItems(0);
}

void GuiSettingsWindow::initPortFwds() {
  int rows = conf_get_str_count(cfg.get(), CONF_portfwd);
  QTableWidget *table = ui->l_portfwd;
  table->clear();
  table->setRowCount(rows);
  table->setColumnCount(2);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->verticalHeader()->hide();
  table->horizontalHeader()->hide();

  const char *value = nullptr;
  char *subkey = nullptr;
  int i = 0;
  while ((value = conf_get_str_strs(cfg.get(), CONF_portfwd, subkey, &subkey))) {
    setTableRow(table, i, subkey, value);
    i++;
  }
  table->sortItems(0);
}

void GuiSettingsWindow::on_pb_portfwd_remove_clicked() {
  QByteArray src = removeCurrentRow(ui->l_portfwd);
  if (!src.isEmpty()) conf_del_str_str(cfg.get(), CONF_portfwd, src);
}

struct PortFwd {
  QString src;
  QString dest;

  bool operator==(const PortFwd &o) const { return src == o.src && dest == o.dest; }
  bool destValid() const {
    auto const parts = dest.split(':');
    return parts.size() == 2 && !parts[0].isEmpty() && !parts[1].isEmpty();
  }
};

static PortFwd getPortFwdFromUi(Ui::GuiSettingsWindow *ui) {
  QString const src = ui->le_tunnel_source->text().trimmed();
  QString const dest = ui->le_tunnel_dest->text().trimmed();
  if (ui->rb_sshport_dynamic->isChecked()) return {"DL" + src, QString{}};
  PortFwd result;
  if (ui->rb_sshport_ipv4->isChecked())
    result.src += "4";
  else if (ui->rb_sshport_ipv6->isChecked())
    result.src += "6";
  if (ui->rb_sshport_local->isChecked())
    result.src += "L";
  else
    result.src += "R";
  result.src += src;
  result.dest = dest;
  return result;
}

void GuiSettingsWindow::on_pb_portfwd_add_clicked() {
  auto fwd = getPortFwdFromUi(ui);
  if (!fwd.destValid()) {
    QMessageBox::critical(
        this, tr("QuTTY Error"),
        tr("You need to specify a destination address in the form \"host.name:port\"."));
    return;
  }

  QTableWidget *table = ui->l_portfwd;
  int const n = table->rowCount();
  for (int i = 0; i < n; i++) {
    PortFwd row;
    row.src = table->item(i, 0)->text();
    row.dest = table->item(i, 1)->text();
    if (row == fwd) {
      QMessageBox::critical(this, tr("QuTTY Error"), tr("Specified forwarding already exists."));
      return;
    }
  }

  auto const src = fwd.src.toLatin1();
  auto const dest = fwd.dest.toLatin1();
  conf_set_str_str(cfg.get(), CONF_portfwd, src, dest);

  int row = table->rowCount();
  table->setRowCount(row + 1);
  setTableRow(table, row, src, dest);
  table->sortItems(0);
}

void GuiSettingsWindow::initHostKeys() {
  auto *list = ui->l_ssh_manual_hostkeys;
  char *subkey = nullptr;
  while (conf_get_str_strs(cfg.get(), CONF_ssh_manual_hostkeys, subkey, &subkey)) {
    list->addItem(subkey);
  }
}

void GuiSettingsWindow::on_pb_ssh_manual_hostkeys_add_clicked() {
  auto hostkey = ui->le_ssh_manual_hostkeys_key->text();
  auto bhostkey = hostkey.toLatin1();
  if (!validate_manual_hostkey(bhostkey.data())) {
    QMessageBox::critical(this, "QuTTY Error", "Host key is not in a valid format");
    return;
  } else if (conf_get_str_str_opt(cfg.get(), CONF_ssh_manual_hostkeys, bhostkey)) {
    QMessageBox::critical(this, "QuTTY Error", "Specified host key is already listed");
    return;
  }

  auto *list = ui->l_ssh_manual_hostkeys;
  list->addItem(hostkey);
  conf_set_str_str(cfg.get(), CONF_ssh_manual_hostkeys, bhostkey, "");
}

void GuiSettingsWindow::on_pb_ssh_manual_hostkeys_remove_clicked() {
  QListWidget *list = ui->l_ssh_manual_hostkeys;
  auto *item = list->currentItem();
  if (!item) return;
  auto text = item->text();
  delete item;
  conf_del_str_str(cfg.get(), CONF_ssh_manual_hostkeys, text.toLatin1());
}

#ifndef NO_GSSAPI

// From putty-0.63/wingss.c
static const DataItem _gsslibnames[3] = {{"MIT Kerberos GSSAPI32.DLL", 0},
                                         {"Microsoft SSPI SECUR32.DLL", 1},
                                         {"User-specified GSSAPI DLL", 2}};

static const DataList gssDataList(CONF_ssh_gsslist, 3, _gsslibnames);

void GuiSettingsWindow::initGSSList() { gssDataList.initList(ui->l_ssh_gsslist, cfg.get()); }

void GuiSettingsWindow::on_pb_ssh_gss_up_clicked() {
  if (listMoveUp(ui->l_ssh_gsslist)) gssDataList.updateConfig(ui->l_ssh_gsslist, cfg.get());
}

void GuiSettingsWindow::on_pb_ssh_gss_down_clicked() {
  if (listMoveDown(ui->l_ssh_gsslist)) gssDataList.updateConfig(ui->l_ssh_gsslist, cfg.get());
}

#endif // NO_GSSAPI

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
  ui->chb_fontsel_varpitch->setText(
      selfont.fixedPitch() ? "The selected font has variable-pitch. Doesn't have fixed-pitch" : "");
}

void GuiSettingsWindow::on_treeWidget_itemSelectionChanged() {
  ui->stackedWidget->setCurrentIndex(
      ui->treeWidget->selectedItems()[0]->data(0, Qt::UserRole).toInt());
}

void GuiSettingsWindow::on_btn_about_clicked() {
  QMessageBox::about(this, "About " APPNAME,
                     APPNAME "\nRelease " QUTTY_RELEASE_VERSION
                             "\n\nhttp://code.google.com/p/qutty/");
}

// From putty-0.63/config.c
static const char *const colours[] = {"Default Foreground", "Default Bold Foreground",
                                      "Default Background", "Default Bold Background",
                                      "Cursor Text",        "Cursor Colour",
                                      "ANSI Black",         "ANSI Black Bold",
                                      "ANSI Red",           "ANSI Red Bold",
                                      "ANSI Green",         "ANSI Green Bold",
                                      "ANSI Yellow",        "ANSI Yellow Bold",
                                      "ANSI Blue",          "ANSI Blue Bold",
                                      "ANSI Magenta",       "ANSI Magenta Bold",
                                      "ANSI Cyan",          "ANSI Cyan Bold",
                                      "ANSI White",         "ANSI White Bold"};

void GuiSettingsWindow::initColours() {
  int row = 0;
  for (const char *colour : colours) {
    ui->l_colour->addItem(colour);
  }
}

static void setColour(Conf *conf, int index, QRgb rgb) {
  conf_set_int_int(conf, CONF_colours, index * 3 + 0, qRed(rgb));
  conf_set_int_int(conf, CONF_colours, index * 3 + 1, qGreen(rgb));
  conf_set_int_int(conf, CONF_colours, index * 3 + 2, qBlue(rgb));
}

static QRgb getColour(Conf *conf, int index) {
  int r = conf_get_int_int(conf, CONF_colours, index * 3 + 0);
  int g = conf_get_int_int(conf, CONF_colours, index * 3 + 1);
  int b = conf_get_int_int(conf, CONF_colours, index * 3 + 2);
  return qRgb(r, g, b);
}

void GuiSettingsWindow::on_btn_colour_modify_clicked() {
  int r = ui->le_colour_r->text().toInt();
  int g = ui->le_colour_g->text().toInt();
  int b = ui->le_colour_b->text().toInt();

  QColor oldcol = QColor(r, g, b);
  QColor newcol = QColorDialog::getColor(oldcol);
  if (newcol.isValid()) {
    int rgb = newcol.rgb();
    ui->le_colour_r->setText(QString::number(qRed(rgb)));
    ui->le_colour_g->setText(QString::number(qGreen(rgb)));
    ui->le_colour_b->setText(QString::number(qBlue(rgb)));
    int curr = ui->l_colour->currentIndex().row();
    if (curr >= 0 && curr < NCFGCOLOURS) setColour(cfg.get(), curr, rgb);
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
    setColour(cfg.get(), prev, qRgb(r, g, b));
  }
  if (curr >= 0 && curr < NCFGCOLOURS) {
    QRgb rgb = getColour(cfg.get(), curr);
    ui->le_colour_r->setText(QString::number(qRed(rgb)));
    ui->le_colour_g->setText(QString::number(qGreen(rgb)));
    ui->le_colour_b->setText(QString::number(qBlue(rgb)));
  }
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

  if (!opt_unsupp.isEmpty())
    QMessageBox::warning(
        NULL, QObject::tr("Qutty Configuration"),
        QObject::tr("Following options are not yet supported in QuTTY.\n\n%1").arg(opt_unsupp));
}

void GuiSettingsWindow::on_btn_sessionlog_filename_browse_clicked() {
  ui->le_sessionlog_filename->setText(QFileDialog::getSaveFileName(
      this, tr("Select session log filename"), "qutty", tr("log files (*.log)")));
}
