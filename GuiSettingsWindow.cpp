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
#include <variant>

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
static Qt::ItemFlags const CONST_FLAGS =
    Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable;

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
#ifndef Q_OS_MACOS
  ui->gb_osx_meta->hide();
#endif
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
  ui->treeWidget->topLevelItem(2)->child(3)->child(0)->setData(0, Qt::UserRole, GUI_PAGE_COPY);
  ui->treeWidget->topLevelItem(2)->child(4)->setData(0, Qt::UserRole, GUI_PAGE_COLOURS);
  ui->treeWidget->topLevelItem(3)->setData(0, Qt::UserRole, GUI_PAGE_CONNECTION);
  ui->treeWidget->topLevelItem(3)->child(0)->setData(0, Qt::UserRole, GUI_PAGE_DATA);
  ui->treeWidget->topLevelItem(3)->child(1)->setData(0, Qt::UserRole, GUI_PAGE_PROXY);
  ui->treeWidget->topLevelItem(3)->child(2)->setData(0, Qt::UserRole, GUI_PAGE_TELNET);
  ui->treeWidget->topLevelItem(3)->child(3)->setData(0, Qt::UserRole, GUI_PAGE_RLOGIN);
  ui->treeWidget->topLevelItem(3)->child(4)->setData(0, Qt::UserRole, GUI_PAGE_SSH);
  ui->treeWidget->topLevelItem(3)->child(4)->child(0)->setData(0, Qt::UserRole, GUI_PAGE_KEX);
  ui->treeWidget->topLevelItem(3)->child(4)->child(1)->setData(0, Qt::UserRole, GUI_PAGE_HOST_KEYS);
  ui->treeWidget->topLevelItem(3)->child(4)->child(2)->setData(0, Qt::UserRole, GUI_PAGE_CIPHER);
  ui->treeWidget->topLevelItem(3)->child(4)->child(3)->setData(0, Qt::UserRole, GUI_PAGE_AUTH);
  ui->treeWidget->topLevelItem(3)->child(4)->child(3)->child(0)->setData(0, Qt::UserRole,
                                                                         GUI_PAGE_GSSAPI);
  ui->treeWidget->topLevelItem(3)->child(4)->child(4)->setData(0, Qt::UserRole, GUI_PAGE_TTY);
  ui->treeWidget->topLevelItem(3)->child(4)->child(5)->setData(0, Qt::UserRole, GUI_PAGE_X11);
  ui->treeWidget->topLevelItem(3)->child(4)->child(6)->setData(0, Qt::UserRole, GUI_PAGE_TUNNELS);
  ui->treeWidget->topLevelItem(3)->child(4)->child(7)->setData(0, Qt::UserRole, GUI_PAGE_BUGS);
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

  initCodepages();
  initWordness();
}

GuiSettingsWindow::~GuiSettingsWindow() { delete ui; }

void GuiSettingsWindow::initCodepages() {
  ui->cb_codepage->clear();
  const char *cpname;
  int i = 0;
  while ((cpname = cp_enumerate(i++))) ui->cb_codepage->addItem(cpname);
}

void GuiSettingsWindow::initWordness() {
  QTableWidget *table = ui->tb_char_classes;
  table->setRowCount(256);
  table->horizontalHeader()->hide();
  for (int ch = 0; ch < 256; ch++) {
    auto *col0 = new QTableWidgetItem(QString::asprintf("(0x%02X)", ch));
    auto *col1 = new QTableWidgetItem((ch >= 32 && ch <= 127) ? QString(QChar(ch)) : QString());
    col0->setFlags(CONST_FLAGS);
    col1->setFlags(CONST_FLAGS);
    table->setItem(ch, 0, col0);
    table->setItem(ch, 1, col1);
  }
}

void GuiSettingsWindow::on_rb_contype_telnet_clicked() {
  ui->sw_target->setCurrentIndex(0);
  ui->le_port->setText("23");
}

void GuiSettingsWindow::on_rb_contype_ssh_clicked() {
  ui->sw_target->setCurrentIndex(0);
  ui->le_port->setText("22");
}

void GuiSettingsWindow::on_rb_contype_serial_clicked() { ui->sw_target->setCurrentIndex(1); }

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
  Optional = 1,
  CurrentText = 2,
  CurrentUserRole3 = 4,
  UserRole0 = 8,
  TwoColumns = 16,
  Col0IsEditable = 32,
  Col0IsSorted = 64,
  Col1IsEditable = 128,
  Col1IsFormatted = 256,
  DontWrite = 512,
  TwoColumnsEditable = TwoColumns | Col0IsEditable | Col1IsEditable,
  TTYMode = TwoColumns | Col0IsSorted | Col1IsFormatted | DontWrite,
};

Q_DECLARE_FLAGS(UIFlags, UI)
Q_DECLARE_OPERATORS_FOR_FLAGS(UIFlags)

struct DataItem {
  const char *text;
  int data;
  DataItem(const char *text, int data = 0) : text(text), data(data) {}
};

struct DataList {
  const DataItem *items;
  size_t count;
  config_primary_key key;

  constexpr DataList(config_primary_key key, int count, const DataItem items[])
      : items(items), count(count), key(key) {}

  void initList(QListWidget *list) const {
    list->clear();
    for (int i = 0; i < count; i++) {
      auto *item = new QListWidgetItem(items[i].text);
      item->setData(Qt::UserRole, items[i].data);
      list->addItem(item);
    }
  }
};

// From putty-0.63/config.c
static const DataItem ciphers[] = {{"ChaCha20 (SSH-2 only)", CIPHER_CHACHA20},
                                   {"3DES", CIPHER_3DES},
                                   {"Blowfish", CIPHER_BLOWFISH},
                                   {"DES", CIPHER_DES},
                                   {"AES (SSH-2 only)", CIPHER_AES},
                                   {"Arcfour (SSH-2 only)", CIPHER_ARCFOUR},
                                   {"-- warn below here --", CIPHER_WARN}};

static const DataItem kexes[] = {{"Diffie-Hellman group 1", KEX_DHGROUP1},
                                 {"Diffie-Hellman group 14", KEX_DHGROUP14},
                                 {"Diffie-Hellman group exchange", KEX_DHGEX},
                                 {"RSA-based key exchange", KEX_RSA},
                                 {"ECDH key exchange", KEX_ECDH},
                                 {"-- warn below here --", KEX_WARN}};

// From putty-0.69/config.c
static const DataItem hks[] = {{"Ed25519", HK_ED25519},
                               {"ECDSA", HK_ECDSA},
                               {"DSA", HK_DSA},
                               {"RSA", HK_RSA},
                               {"-- warn below here --", HK_WARN}};

// From putty-0.63/windows/wingss.c
static const DataItem _gsslibnames[3] = {{"MIT Kerberos GSSAPI32.DLL", 0},
                                         {"Microsoft SSPI SECUR32.DLL", 1},
                                         {"User-specified GSSAPI DLL", 2}};

// From putty-0.63/config.c
static const DataItem colours[] = {"Default Foreground", "Default Bold Foreground",
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

static const DataList cipherDataList(CONF_ssh_cipherlist, CIPHER_MAX, ciphers);
static const DataList kexDataList(CONF_ssh_kexlist, KEX_MAX, kexes);
static const DataList hkDataList(CONF_ssh_hklist, HK_MAX, hks);
static const DataList gssDataList(CONF_ssh_gsslist, 3, _gsslibnames);
static const DataList coloursDataList(CONF_colours, sizeof(colours) / sizeof(colours[0]), colours);

QString formatTTYMode(const QString &value);

#define UI_MAPPING(X)                                                          \
  /* control name, keyword, value (opt) */                                     \
                                                                               \
  X("l_saved_sess", config_name, UI::CurrentUserRole3)                         \
  /* Session */                                                                \
  X("le_hostname", host)                                                       \
  X("le_port", port)                                                           \
  X("le_line", serline)                                                        \
  X("le_speed", serspeed)                                                      \
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
  X("chb_logheader", logheader)                                                \
  X("chb_sessionlog_omitpasswd", logomitpass)                                  \
  X("chb_sessionlog_omitdata", logomitdata)                                    \
  /* Terminal Emulation */                                                     \
  X("chb_terminaloption_autowrap", wrap_mode)                                  \
  X("chb_terminaloption_decorigin", dec_om)                                    \
  X("chb_terminaloption_cr", lfhascr)                                          \
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
  X("rb_ininumkeys_normal", app_keypad, false)                                 \
  X("rb_ininumkeys_app", app_keypad, true)                                     \
  X("rb_ininumkeys_nethack", nethack_keypad, true)                             \
  X("chb_altgrkey", compose_key)                                               \
  X("chb_ctrl_alt", ctrlaltkeys)                                               \
  X("chb_osx_option_meta", osx_option_meta, UI::Optional)                      \
  X("chb_osx_command_meta", osx_command_meta, UI::Optional)                    \
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
  X("chb_no_remote_clearscroll", no_remote_clearscroll)                        \
  X("rb_featqtitle_none", remote_qtitle_action, TITLE_NONE)                    \
  X("rb_featqtitle_empstring", remote_qtitle_action, TITLE_EMPTY)              \
  X("rb_featqtitle_wndtitle", remote_qtitle_action, TITLE_REAL)                \
  X("chb_no_dbackspace", no_dbackspace)                                        \
  X("chb_no_remote_charset", no_remote_charset)                                \
  X("chb_no_arabic", no_arabicshaping)                                         \
  X("chb_no_bidi", no_bidi)                                                    \
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
  X("chb_scrollbar_on_left", scrollbar_on_left)                                \
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
  X("cb_codepage", line_codepage, UI::CurrentText)                             \
  X("chb_utf8_override", utf8_override)                                        \
  X("chb_cjk_ambig_wide", cjk_ambig_wide)                                      \
  X("chb_xlat_capslockcyr", xlat_capslockcyr)                                  \
  X("rb_translation_useunicode", vtmode, VT_UNICODE)                           \
  X("rb_translation_poorman", vtmode, VT_POORMAN)                              \
  X("rb_translation_xwindows", vtmode, VT_XWINDOWS)                            \
  X("rb_translation_ansi_oem", vtmode, VT_OEMANSI)                             \
  X("rb_translation_oem", vtmode, VT_OEMONLY)                                  \
  X("chb_translation_copy_paste", rawcnp)                                      \
  X("chb_utf8linedraw", utf8linedraw)                                          \
  /* Selection */                                                              \
  X("rb_cpmouseaction_windows", mouse_is_xterm, 0)                             \
  X("rb_cpmouseaction_compromise", mouse_is_xterm, 2) /* TODO chk */           \
  X("rb_cpmouseaction_xterm", mouse_is_xterm, 1)                               \
  X("chb_cpmouseaction_shift", mouse_override)                                 \
  X("rb_cpmouseaction_defaultnormal", rect_select, 0)                          \
  X("rb_cpmouseaction_defaultrect", rect_select, 1)                            \
  X("chb_mouseautocopy", mouseautocopy)                                        \
  X("cb_mousepaste", mousepaste)                                               \
  X("cb_ctrlshiftins", ctrlshiftins)                                           \
  X("cb_ctrlshiftcv", ctrlshiftcv)                                             \
  X("chb_paste_controls", paste_controls)                                      \
  /* Copy */                                                                   \
  X("l_char_classes", wordness)                                                \
  X("chb_rtf_paste", rtf_paste)                                                \
  /* Colours */                                                                \
  X("chb_coloursoption_ansi", ansi_colour)                                     \
  X("chb_coloursoption_xterm", xterm_256_colour)                               \
  X("chb_true_colour", true_colour)                                            \
  X("rb_bold_font", bold_style, 1)                                             \
  X("rb_bold_colour", bold_style, 2)                                           \
  X("rb_bold_both", bold_style, 3)                                             \
  X("chb_colouroption_palette", try_palette)                                   \
  X("chb_colouroption_usesystem", system_colour)                               \
  X("l_colour", colours, &coloursDataList)                                     \
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
  X("tb_env_vars", environmt, UI::TwoColumnsEditable)                          \
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
  X("rb_proxy_log_to_term_no", proxy_log_to_term, FORCE_OFF)                   \
  X("rb_proxy_log_to_term_yes", proxy_log_to_term, FORCE_ON)                   \
  X("rb_proxy_log_to_term_until_start", proxy_log_to_term, AUTO)               \
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
  X("rb_sshprotocol_1_only", sshprot, 0)                                       \
  X("rb_sshprotocol_2_only", sshprot, 3)                                       \
  X("cb_ssh_connection_sharing", ssh_connection_sharing)                       \
  X("cb_ssh_connection_sharing_upstream", ssh_connection_sharing_upstream)     \
  X("cb_ssh_connection_sharing_downstream", ssh_connection_sharing_downstream) \
  /* SSH Key Exchange */                                                       \
  X("l_ssh_kexlist", ssh_kexlist, &kexDataList, UI::UserRole0)                 \
  X("chb_try_gssapi_kex_1", try_gssapi_kex)                                    \
  X("le_ssh_rekey_time", ssh_rekey_time)                                       \
  X("le_gssapirekey", gssapirekey)                                             \
  X("le_ssh_rekey_data", ssh_rekey_data)                                       \
  /* SSH Host Keys */                                                          \
  X("l_ssh_hklist", ssh_hklist, &hkDataList, UI::UserRole0)                    \
  X("chb_ssh_prefer_known_hostkeys", ssh_prefer_known_hostkeys)                \
  X("l_ssh_manual_hostkeys", ssh_manual_hostkeys)                              \
  /* SSH Cipher */                                                             \
  X("l_ssh_cipherlist", ssh_cipherlist, &cipherDataList, UI::UserRole0)        \
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
  X("chb_try_gssapi_auth", try_gssapi_auth)                                    \
  X("chb_try_gssapi_kex", try_gssapi_kex)                                      \
  X("chb_gssapi_credential", gssapifwd)                                        \
  X("l_ssh_gsslist", ssh_gsslist, &gssDataList, UI::UserRole0)                 \
  X("le_ssh_gss_custom", ssh_gss_custom, UI::Optional)                         \
  /* Remote Terminal */                                                        \
  X("chb_nopty", nopty)                                                        \
  X("tb_ttymodes", ttymodes, formatTTYMode, UI::TTYMode)                       \
  /* X11 */                                                                    \
  X("chb_x11_forward", x11_forward)                                            \
  X("le_x11_display", x11_display)                                             \
  X("rb_x11remote_mit", x11_auth, X11_MIT)                                     \
  X("rb_x11remote_xdm", x11_auth, X11_XDM)                                     \
  X("le_xauthfile", xauthfile)                                                 \
  /* SSH Port Forwarding */                                                    \
  X("chb_lport_acceptall", lport_acceptall)                                    \
  X("chb_rport_acceptall", rport_acceptall)                                    \
  X("tb_portfwd", portfwd, UI::TwoColumns | UI::Col0IsSorted)                  \
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
  X("cb_config_flow", serflow)

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

enum Type : uint8_t {
  TYPE_NONE,
  TYPE_STR,
  TYPE_STR_NONE = TYPE_STR,
  TYPE_INT,
  TYPE_INT_NONE = TYPE_INT,
  TYPE_BOOL,
  TYPE_BOOL_NONE = TYPE_BOOL,
  TYPE_FILENAME,
  TYPE_FILENAME_NONE = TYPE_FILENAME,
  TYPE_FONT,
  TYPE_FONT_NONE = TYPE_FONT,
  TYPE_INT_INT,
  TYPE_STR_STR,
};

static const char *typeName[] = {"None",     "str",  "int",     "bool",
                                 "filename", "font", "int/int", "str/str"};

extern const std::unordered_map<config_primary_key, Type> KEY_TO_TYPE;

using UIFormatter = QString (*)(const QString &);
using KVValue = std::variant<bool, int, std::vector<int>, const DataList *, UIFormatter>;

#define UI_TO_KEY_ENTRY(ui, key, ...) {ui, {CONF_##key __VA_OPT__(, ) __VA_ARGS__}},
struct KeyValue {
  KVValue _value;
  config_primary_key key;
  UIFlags options = {};
  Type const type = KEY_TO_TYPE.at(key);

  KeyValue(config_primary_key key, std::initializer_list<int> cb_values, UIFlags opt = {})
      : key(key), _value(cb_values), options(opt) {}
  KeyValue(config_primary_key key, const DataList *dataList, UIFlags opt = {})
      : key(key), _value(dataList), options(opt) {}
  KeyValue(config_primary_key key, UIFormatter formatter, UIFlags opt = {})
      : key(key), _value(formatter), options(opt) {}
  KeyValue(config_primary_key key, bool value, UIFlags opt = {})
      : key(key), _value(value), options(opt) {}
  KeyValue(config_primary_key key, int value, UIFlags opt = {})
      : key(key), _value(value), options(opt) {}
  KeyValue(config_primary_key key, UIFlags opt = {}) : key(key), options(opt) {}

  template <typename T>
  bool holds() const {
    return std::holds_alternative<T>(_value);
  }
  template <typename T>
  T value() const {
    return std::get<T>(_value);
  }
};

const KeyValue *kvForWidget(QWidget *w) {
  static const std::unordered_map<std::string, KeyValue> UI_TO_KV = {UI_MAPPING(UI_TO_KEY_ENTRY)};
#ifdef _DEBUG
  static const QVector<std::string> IGNORE_TYPES = {
      "QLabel", "QGroupBox",   "QWidget",        "QScrollBar",  "QDialogButtonBox",
      "QFrame", "QScrollArea", "QStackedWidget", "QPushButton",
  };
#endif
  auto ikv = UI_TO_KV.find(w->objectName().toStdString());
  if (ikv == UI_TO_KV.end()) {
#ifdef _DEBUG
    const char *className = w->metaObject()->className();
    bool ignoredType = IGNORE_TYPES.contains(className);
    if (!w->objectName().isEmpty() && !ignoredType)
      qDebug() << "no kv for" << className << w->objectName();
#endif
    return nullptr;
  }
  return &ikv->second;
}

static void setTableRow(QTableWidget *table, int row, const QString &left, const QString &right) {
  const KeyValue *kv = kvForWidget(table);
  if (!left.isNull()) {
    auto *col0 = new QTableWidgetItem(left);
    if (!(kv->options & UI::Col0IsEditable)) col0->setFlags(CONST_FLAGS);
    table->setItem(row, 0, col0);
  }
  if (!right.isNull()) {
    auto *col1 = new QTableWidgetItem(right);
    if (!(kv->options & UI::Col1IsEditable)) col1->setFlags(CONST_FLAGS);
    table->setItem(row, 1, col1);
  }
}

static bool contains(QListWidget *lw, const QString &text) {
  for (int i = 0; i < lw->count(); i++) {
    auto *item = lw->item(i);
    if (item && item->text() == text) return true;
  }
  return false;
}

static void unhandledEntry(const QWidget *w, const KeyValue &kv, bool store) {
  qDebug() << "!!" << w->metaObject()->className() << w->objectName() << "is"
           << (store ? "stored to" : "loaded from") << "an unhandled type" << typeName[kv.type];
}

template <typename T>
static bool set(QWidget *w, Conf *conf, const KeyValue &kv) {
  T *concrete = qobject_cast<T *>(w);
  if (concrete) return setW(concrete, conf, kv);
  return false;
}

static bool setW(QCheckBox *chb, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_BOOL) {
    chb->setChecked(conf_get_bool(conf, kv.key));
    return true;
  }
  return false;
}

static bool setW(QRadioButton *rb, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_INT || kv.type == TYPE_BOOL) {
    int cv = 0;
    if (kv.type == TYPE_INT)
      cv = conf_get_int(conf, kv.key);
    else
      cv = conf_get_bool(conf, kv.key);
    if (kv.holds<bool>()) {
      bool val = kv.value<bool>();
      if (cv && val || !cv && !val) rb->click();
      return true;
    }
    if (kv.holds<int>()) {
      int val = kv.value<int>();
      if (cv == val) rb->click();
      return true;
    }
  }
  return false;
}

static bool setW(QLineEdit *le, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_STR) {
    le->setText(conf_get_str(conf, kv.key));
    return true;
  } else if (kv.type == TYPE_INT) {
    le->setText(QString::number(conf_get_int(conf, kv.key)));
    return true;
  } else if (kv.type == TYPE_FILENAME) {
    le->setText(conf_get_filename(conf, kv.key)->path);
    return true;
  }
  return false;
}

static bool setW(QComboBox *cb, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_INT) {
    cb->setCurrentIndex(conf_get_int(conf, kv.key));
    return true;
  } else if (kv.type == TYPE_STR) {
    int ind = cb->findText(conf_get_str(conf, kv.key));
    if (ind == -1) ind = 0;
    cb->setCurrentIndex(ind);
    return true;
  }
  return false;
}

static bool setW(QListWidget *lw, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_INT_INT && kv.holds<const DataList *>()) {
    auto &dataList = *kv.value<const DataList *>();
    if (kv.options & UI::UserRole0) {
      // priority ordered lists
      std::unordered_map<int, QListWidgetItem *> items;
      for (int i = 0; i < dataList.count; i++) {
        int data = dataList.items[i].data;
        auto *item = new QListWidgetItem(dataList.items[i].text);
        item->setData(Qt::UserRole, data);
        items[data] = item;
      }
      lw->clear();
      for (int i = 0; i < items.size(); i++) {
        int value = conf_get_int_int(conf, kv.key, i);
        auto it = items.find(value);
        if (it != items.end()) lw->addItem(it->second);
      }
    } else {
      for (int i = 0; i < dataList.count; i++) lw->addItem(dataList.items[i].text);
    }
    return true;
  } else if (kv.type == TYPE_STR_STR) {
    lw->clear();
    char *subkey = nullptr;
    while (conf_get_str_strs(conf, kv.key, subkey, &subkey)) lw->addItem(subkey);
    return true;
  }
  return false;
}

static bool setW(QButtonGroup *bg, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_INT) {
    int val = conf_get_int(conf, kv.key);
    if (val > 0 && val < bg->buttons().size()) bg->button(val)->click();
    return true;
  }
  return false;
}

static int conf_get_str_count(Conf *conf, config_primary_key key) {
  int rows = 0;
  char *subkey = nullptr;
  while (conf_get_str_strs(conf, key, subkey, &subkey)) rows++;
  return rows;
}

static bool setW(QTableWidget *tw, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_STR_STR && kv.options & UI::TwoColumns) {
    tw->setRowCount(0);  // tw->clear() would retain dimensions
    tw->verticalHeader()->hide();
    tw->horizontalHeader()->hide();

    char *value = nullptr;
    char *subkey = nullptr;
    int row = 0;
    UIFormatter formatter = kv.holds<UIFormatter>() ? kv.value<UIFormatter>() : nullptr;
    while ((value = conf_get_str_strs(conf, kv.key, subkey, &subkey))) {
      tw->insertRow(row);
      if (kv.options & UI::Col1IsFormatted)
        setTableRow(tw, row, subkey, formatter(value).toLatin1());
      else
        setTableRow(tw, row, subkey, value);
      row++;
    }
    if (kv.options & UI::Col0IsSorted) tw->sortItems(0);
    return true;
  }
  return false;
}

#define KEY_TO_TYPE_ENTRY(value, subkey, key) {CONF_##key, TYPE_##value##_##subkey},
static const std::unordered_map<config_primary_key, Type> KEY_TO_TYPE = {
    CONFIG_OPTIONS(KEY_TO_TYPE_ENTRY)};

void GuiSettingsWindow::setConfig(QtConfig::Pointer &&_cfg) {
  this->cfg = std::move(_cfg);

  for (QWidget *widget : findChildren<QWidget *>()) {
    const KeyValue *kv = kvForWidget(widget);
    if (!kv) continue;
    if (kv->options & UI::Optional && !confKeyExists(cfg.get(), kv->key)) continue;
    if (set<QCheckBox>(widget, cfg.get(), *kv)) continue;
    if (set<QRadioButton>(widget, cfg.get(), *kv)) continue;
    if (set<QLineEdit>(widget, cfg.get(), *kv)) continue;
    if (set<QComboBox>(widget, cfg.get(), *kv)) continue;
    if (set<QListWidget>(widget, cfg.get(), *kv)) continue;
    if (set<QTableWidget>(widget, cfg.get(), *kv)) continue;
    if (set<QButtonGroup>(widget, cfg.get(), *kv)) continue;
    unhandledEntry(widget, *kv, false);
  }

  setWordness();

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
}

template <typename T>
static bool get(QWidget *w, Conf *conf, const KeyValue &kv) {
  T *concrete = qobject_cast<T *>(w);
  if (concrete) return getW(concrete, conf, kv);
  return false;
}

static bool getW(QCheckBox *chb, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_BOOL) {
    conf_set_bool(conf, kv.key, chb->isChecked());
    return true;
  }
  return false;
}

static bool getW(QRadioButton *rb, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_INT && kv.holds<int>()) {
    if (rb->isChecked()) conf_set_int(conf, kv.key, kv.value<int>());
    return true;
  }
  if (kv.type == TYPE_BOOL && kv.holds<bool>()) {
    if (rb->isChecked()) conf_set_bool(conf, kv.key, kv.value<bool>());
    return true;
  }
  return false;
}

static bool getW(QLineEdit *le, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_STR) {
    QByteArray text = le->text().toUtf8();
    conf_set_str(conf, kv.key, text.data());
    return true;
  }
  if (kv.type == TYPE_INT) {
    bool ok;
    int i = le->text().toInt(&ok);
    if (ok) conf_set_int(conf, kv.key, i);
    return true;
  }
  if (kv.type == TYPE_FILENAME) {
    Filename *fn = conf_get_filename(conf, kv.key);
    qstring_to_char(fn->path, le->text(), sizeof(fn->path));
    conf_set_filename(conf, kv.key, fn);
    return true;
  }
  return false;
}

static bool getW(QComboBox *cb, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_STR && (kv.options & UI::CurrentText)) {
    QByteArray text = cb->currentText().toUtf8();
    conf_set_str(conf, kv.key, text.data());
    return true;
  }
  if (kv.type == TYPE_INT) {
    int i = cb->currentIndex();
    if (i >= 0) conf_set_int(conf, kv.key, i);
    return true;
  }
  return false;
}

static bool getW(QTreeWidget *tw, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_STR && (kv.options & UI::CurrentUserRole3)) {
    QByteArray text = tw->currentItem()->data(0, 3).toString().toUtf8();
    conf_set_str(conf, kv.key, text);
    return true;
  }
  return false;
}

static bool getW(QListWidget *lw, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_STR) {
    QByteArray text = lw->currentItem()->text().toUtf8();
    conf_set_str(conf, kv.key, text.data());
    return true;
  } else if (kv.type == TYPE_INT_INT && (kv.options & UI::UserRole0)) {
    for (int i = 0; i < lw->count(); i++) {
      bool ok;
      int value = lw->item(i)->data(Qt::UserRole).toInt(&ok);
      if (ok) conf_set_int_int(conf, kv.key, i, value);
    }
    return true;
  }
  return false;
}

static bool getW(QTableWidget *tw, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_STR_STR && kv.options & UI::TwoColumns) {
    if (kv.options & UI::DontWrite) return true;
    std::unordered_set<QString> confVars;
    char *varName = nullptr;
    while (conf_get_str_strs(conf, kv.key, varName, &varName)) {
      confVars.insert(varName);
    }
    int rows = conf_get_str_count(conf, kv.key);
    for (int i = 0; i < tw->rowCount(); i++) {
      auto col0 = tw->item(i, 0)->text().toLatin1();
      auto col1 = tw->item(i, 1)->text().toLatin1();
      conf_set_str_str(conf, kv.key, col0, col1);
      confVars.erase(col0);
    }
    for (const QString &varName : confVars) {
      conf_del_str_str(conf, kv.key, varName.toLatin1());
    }
    return true;
  }
  return false;
}

static bool getW(QButtonGroup *bg, Conf *conf, const KeyValue &kv) {
  if (kv.type == TYPE_INT) {
    int val = bg->checkedId();
    if (val > 0) conf_set_int(conf, kv.key, val);
    return true;
  }
  return false;
}

Conf *GuiSettingsWindow::getConfig() {
  for (QWidget *const widget : findChildren<QWidget *>()) {
    const KeyValue *kv = kvForWidget(widget);
    if (!kv) continue;
    if (get<QCheckBox>(widget, cfg.get(), *kv)) continue;
    if (get<QRadioButton>(widget, cfg.get(), *kv)) continue;
    if (get<QLineEdit>(widget, cfg.get(), *kv)) continue;
    if (get<QComboBox>(widget, cfg.get(), *kv)) continue;
    if (get<QTreeWidget>(widget, cfg.get(), *kv)) continue;
    if (get<QListWidget>(widget, cfg.get(), *kv)) continue;
    if (get<QTableWidget>(widget, cfg.get(), *kv)) continue;
    if (get<QButtonGroup>(widget, cfg.get(), *kv)) continue;
    unhandledEntry(widget, *kv, true);
  }
  return cfg.get();
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

void GuiSettingsWindow::setWordness() {
  for (int ch = 0; ch < 256; ch++) {
    int wordness = conf_get_int_int(cfg.get(), CONF_wordness, ch);
    auto *col2 = new QTableWidgetItem(QString::number(wordness));
    ui->tb_char_classes->setItem(ch, 2, col2);
  }
}

void GuiSettingsWindow::on_tb_char_classes_currentItemChanged(QTableWidgetItem *item) {
  item = ui->tb_char_classes->item(item->row(), 2);
  ui->le_char_class->setText(item->text());
}

void GuiSettingsWindow::on_pb_char_class_set_clicked() {
  bool ok;
  int cclass = ui->le_char_class->text().toInt(&ok);
  if (ok) {
    QTableWidget *table = ui->tb_char_classes;
    int ch = table->currentRow();
    table->item(ch, 2)->setText(QString::number(cclass));
    conf_set_int_int(cfg.get(), CONF_wordness, ch, cclass);
  }
}

void GuiSettingsWindow::on_pb_env_add_clicked() {
  QTableWidget *table = ui->tb_env_vars;
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
  if (newRow) table->insertRow(row);
  setTableRow(table, row, name, value);
  ui->le_env_var->clear();
  ui->le_env_value->clear();
}

void GuiSettingsWindow::on_pb_env_remove_clicked() {
  QTableWidget *table = ui->tb_env_vars;
  auto *item = table->currentItem();
  if (!item) return;
  int row = item->row();
  table->removeRow(row);
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

void GuiSettingsWindow::on_pb_ssh_cipher_up_clicked() { listMoveUp(ui->l_ssh_cipherlist); }

void GuiSettingsWindow::on_pb_ssh_cipher_down_clicked() { listMoveDown(ui->l_ssh_cipherlist); }

void GuiSettingsWindow::on_pb_ssh_kex_up_clicked() { listMoveUp(ui->l_ssh_kexlist); }

void GuiSettingsWindow::on_pb_ssh_kex_down_clicked() { listMoveDown(ui->l_ssh_kexlist); }

void GuiSettingsWindow::on_pb_ssh_hklist_up_clicked() { listMoveUp(ui->l_ssh_hklist); }

void GuiSettingsWindow::on_pb_ssh_hklist_down_clicked() { listMoveDown(ui->l_ssh_hklist); }

static QString formatTTYMode(const QString &value) {
  if (value == "A") return "(auto)";
  if (value.startsWith("V")) return value.mid(1);
  if (value.startsWith("N")) return "(don't send)";
  return "(---)";  // should not happen
}

void GuiSettingsWindow::on_tb_ttymodes_currentItemChanged(QTableWidgetItem *current) {
  if (!current) return;
  QTableWidget *table = ui->tb_ttymodes;
  int row = table->indexFromItem(current).row();
  if (row < 0) return;
  QString const name = table->item(row, 0)->text();
  QByteArray const bname = name.toLatin1();
  QLatin1StringView value(conf_get_str_str(cfg.get(), CONF_ttymodes, bname));
  if (value == "A")
    ui->rb_terminalvalue_auto->click();
  else if (value == "N")
    ui->rb_terminalvalue_nothing->click();
  else if (!value.empty() && value[0] == 'V') {
    QString text = value.slice(1);
    ui->le_ttymode->setText(text);
    ui->rb_terminalvalue_this->click();
  }
}

void GuiSettingsWindow::on_pb_ttymodes_set_clicked() {
  QTableWidget *table = ui->tb_ttymodes;
  int row = table->currentRow();
  if (row < 0) return;
  QString const name = table->item(row, 0)->text();
  QByteArray const bname = name.toLatin1();

  QString value = "A";
  if (ui->rb_terminalvalue_this->isChecked()) value = "V" + ui->le_ttymode->text();
  else if (ui->rb_terminalvalue_nothing->isChecked())
    value = "N";
  QByteArray bvalue = value.toLatin1();

  conf_set_str_str(cfg.get(), CONF_ttymodes, bname, bvalue);

  value = formatTTYMode(value);
  bvalue = value.toLatin1();
  setTableRow(table, row, {}, bvalue);
  table->sortItems(0);
}

void GuiSettingsWindow::on_pb_portfwd_remove_clicked() {
  QTableWidget *table = ui->tb_portfwd;
  int row = table->currentRow();
  if (row >= 0) table->removeRow(row);
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

  QTableWidget *table = ui->tb_portfwd;
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
  int row = table->rowCount();
  table->insertRow(row);
  setTableRow(table, row, src, dest);
  table->sortItems(0);
}

void GuiSettingsWindow::on_pb_ssh_manual_hostkeys_add_clicked() {
  auto hostkey = ui->le_ssh_manual_hostkeys_key->text();
  auto bhostkey = hostkey.toLatin1();
  if (!validate_manual_hostkey(bhostkey.data())) {
    QMessageBox::critical(this, "QuTTY Error", "Host key is not in a valid format");
    return;
  } else if (contains(ui->l_ssh_manual_hostkeys, hostkey)) {
    QMessageBox::critical(this, "QuTTY Error", "Specified host key is already listed");
    return;
  }
  ui->l_ssh_manual_hostkeys->addItem(hostkey);
}

void GuiSettingsWindow::on_pb_ssh_manual_hostkeys_remove_clicked() {
  delete ui->l_ssh_manual_hostkeys->currentItem();
}

#ifndef NO_GSSAPI

void GuiSettingsWindow::on_pb_ssh_gss_up_clicked() { listMoveUp(ui->l_ssh_gsslist); }

void GuiSettingsWindow::on_pb_ssh_gss_down_clicked() { listMoveDown(ui->l_ssh_gsslist); }

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
