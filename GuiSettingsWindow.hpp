/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef GUISETTINGSWINDOW_H
#define GUISETTINGSWINDOW_H

#include <QButtonGroup>
#include <QDialog>
#include <QFileDialog>

#include "GuiBase.hpp"
#include "QtConfig.hpp"
extern "C" {
#include "putty.h"
}

namespace Ui {
class GuiSettingsWindow;
}

class QListWidgetItem;
class QTreeWidgetItem;
class QTableWidgetItem;

class GuiSettingsWindow : public QDialog {
  Q_OBJECT
  enum {
    GUI_PAGE_SESSION,
    GUI_PAGE_LOGGING,
    GUI_PAGE_TERMINAL,
    GUI_PAGE_KEYBOARD,
    GUI_PAGE_BELL,
    GUI_PAGE_FEATURES,
    GUI_PAGE_WINDOW,
    GUI_PAGE_APPEARANCE,
    GUI_PAGE_BEHAVIOUR,
    GUI_PAGE_TRANSLATION,
    GUI_PAGE_SELECTION,
    GUI_PAGE_COPY,
    GUI_PAGE_COLOURS,
    GUI_PAGE_CONNECTION,
    GUI_PAGE_DATA,
    GUI_PAGE_PROXY,
    GUI_PAGE_SSH,
    GUI_PAGE_KEX,
    GUI_PAGE_HOST_KEYS,
    GUI_PAGE_CIPHER,
    GUI_PAGE_AUTH,
    GUI_PAGE_GSSAPI,
    GUI_PAGE_TTY,
    GUI_PAGE_X11,
    GUI_PAGE_TUNNELS,
    GUI_PAGE_BUGS,
    GUI_PAGE_SERIAL,
    GUI_PAGE_TELNET,
    GUI_PAGE_RLOGIN,
    GUI_PAGE_SUPDUP,
  } gui_page_names_t;
  enum {
    GUI_LOGLVL_NONE,
    GUI_LOGLVL_PRINT_OUT,
    GUI_LOGLVL_ALL_SES_OUT,
    GUI_LOGLVL_SSH_PACKET,
    GUI_LOGLVL_SSH_RAWDATA
  } gui_loglevel_t;

  // config that is loaded onto the settings window
  QtConfig::Pointer cfg;
  bool isChangeSettingsMode = false;
  GuiTerminalWindow *termWnd = nullptr;  // terminal for which 'change settings' happens
  GuiBase::SplitType openMode = {};
  bool pending_session_changes = false;

 public:
  explicit GuiSettingsWindow(QWidget *parent, GuiBase::SplitType openmode = GuiBase::TYPE_LEAF);
  ~GuiSettingsWindow() override;

  // getter/setter to config in the settings window
  void setConfig(QtConfig::Pointer &&_cfg);
  Conf *getConfig();

  void loadSessionNames();
  void loadInitialSettings(Conf *);
  void enableModeChangeSettings(QtConfig::Pointer &&cfg, GuiTerminalWindow *termWnd);

 signals:
  void signal_session_open(Conf *cgg, GuiBase::SplitType splittype);
  void signal_session_change(Conf *cfg, GuiTerminalWindow *termWnd);
  void signal_session_close();

 private slots:
  void on_buttonBox_accepted();

  void on_buttonBox_rejected();

  void slot_GuiSettingsWindow_rejected();

  void on_rb_contype_telnet_clicked();
  void on_rb_contype_ssh_clicked();
  void on_rb_contype_serial_clicked();

  void on_b_save_sess_clicked();

  void on_b_delete_sess_clicked();

  void on_l_saved_sess_doubleClicked(const QModelIndex &index);
  void on_l_saved_sess_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
  void on_b_sess_copy_clicked();
  void on_btn_sessionlog_filename_browse_clicked();
  void slot_sessname_hierarchy_changed(QTreeWidgetItem *item);

  void on_btn_ssh_auth_browse_keyfile_clicked();

  void on_btn_fontsel_clicked();

  void on_treeWidget_itemSelectionChanged();

  void on_btn_about_clicked();

  void on_l_colour_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void on_btn_colour_modify_clicked();

  void on_tb_char_classes_currentItemChanged(QTableWidgetItem *item);
  void on_pb_char_class_set_clicked();

  void on_pb_env_add_clicked();
  void on_pb_env_remove_clicked();

  void on_pb_ssh_cipher_up_clicked();
  void on_pb_ssh_cipher_down_clicked();

  void on_pb_ssh_kex_up_clicked();
  void on_pb_ssh_kex_down_clicked();

  void on_pb_ssh_hklist_up_clicked();
  void on_pb_ssh_hklist_down_clicked();

  void on_tb_ttymodes_currentItemChanged(QTableWidgetItem *current);
  void on_pb_ttymodes_set_clicked();

  void on_pb_portfwd_add_clicked();
  void on_pb_portfwd_remove_clicked();

  void on_pb_ssh_manual_hostkeys_add_clicked();
  void on_pb_ssh_manual_hostkeys_remove_clicked();

#ifndef NO_GSSAPI
  void on_pb_ssh_gss_up_clicked();
  void on_pb_ssh_gss_down_clicked();
#endif

 private:
  Ui::GuiSettingsWindow *ui;

  void initCodepages();
  void initWordness();

  void setWordness();

  void saveConfigChanges();
};

void chkUnsupportedConfigs(Conf *cfg);

#endif  // GUISETTINGSWINDOW_H
