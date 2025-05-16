#include "GuiCompactSettingsWindow.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStringList>

#include "GuiMainWindow.hpp"
#include "QtCompleterWithAdvancedCompletion.hpp"
#include "QtSessionTreeModel.hpp"
#include "serialize/QtMRUSessionList.hpp"

using std::map;

GuiCompactSettingsWindow::GuiCompactSettingsWindow(QWidget *parent, GuiBase::SplitType openmode)
    : QDialog(parent) {
  setModal(true);
  openMode = openmode;

  QPushButton *details = new QPushButton(tr("Details"));
  connect(details, SIGNAL(clicked()), this, SLOT(on_details_clicked()));

  QDialogButtonBox *btn_box = new QDialogButtonBox(QDialogButtonBox::Cancel);

  btn_box->addButton(tr("Open"), QDialogButtonBox::AcceptRole);
  connect(btn_box, SIGNAL(accepted()), this, SLOT(on_open_clicked()));
  connect(btn_box, SIGNAL(rejected()), this, SLOT(on_close_clicked()));
  connect(this, SIGNAL(rejected()), this, SLOT(on_close_clicked()));

  cb_hostname.setMinimumWidth(500);

  QStringList completions;
  for (auto it = qutty_mru_sesslist.mru_list.begin(); it != qutty_mru_sesslist.mru_list.end();
       ++it) {
    if (it->second.isEmpty() || it->second[0] == QChar('\0')) continue;
    // in 'hostname|sessname' format
    completions << it->second + "|" + it->first;
  }
  hostname_completer = new QtCompleterWithAdvancedCompletion(&cb_hostname);
  hostname_completer->setModel(completions);
  hostname_completer->popup()->setItemDelegate(new QtHostNameCompleterItemDelegate);
  connect(hostname_completer, SIGNAL(activated(QString)), this,
          SLOT(on_hostname_completion_activated(QString)));
  cb_hostname.setItemDelegate(new QtHostNameCompleterItemDelegate);
  connect(&cb_hostname, SIGNAL(textActivated(QString)), SLOT(on_cb_hostname_activated(QString)));

  cb_session_list.setItemDelegate(new QtSessionTreeItemDelegate);
  session_list_model = new QtSessionTreeModel(this, qutty_config.config_list);
  cb_session_list.setModel(session_list_model);
  cb_session_list.setMaxVisibleItems(25);

  cb_connection_type.setMaximumWidth(100);
  cb_connection_type.addItem("Telnet");
  cb_connection_type.addItem("SSH");

  if (qutty_mru_sesslist.mru_list.size() > 0 &&
      qutty_config.config_list.find(qutty_mru_sesslist.mru_list[0].first) !=
          qutty_config.config_list.end() &&
      !completions.isEmpty()) {
    QString sessname = qutty_mru_sesslist.mru_list[0].first;
    on_hostname_completion_activated(completions.at(0));
    setConnectionType(conf_get_int(qutty_config.config_list[sessname].get(), CONF_protocol));
  } else if (qutty_config.config_list.find(QUTTY_DEFAULT_CONFIG_SETTINGS) !=
             qutty_config.config_list.end()) {
    Conf *cfg = qutty_config.config_list[QUTTY_DEFAULT_CONFIG_SETTINGS].get();
    cb_session_list.setCurrentIndex(cb_session_list.findText(QUTTY_DEFAULT_CONFIG_SETTINGS));
    hostname_completer->setText(conf_get_str(cfg, CONF_host));
    setConnectionType(conf_get_int(cfg, CONF_protocol));
  }

  connect(&cb_session_list, SIGNAL(activated(int)), this, SLOT(on_cb_session_list_activated(int)));

  QGridLayout *layout = new QGridLayout(this);

  layout->addWidget(new QLabel("Hostname : ", this));
  layout->addWidget(&cb_hostname);

  layout->addWidget(new QLabel("Session profiles : ", this));
  layout->addWidget(&cb_session_list);

  layout->addWidget(new QLabel("Connection type : ", this));
  layout->addWidget(&cb_connection_type);

  QHBoxLayout *hlayout = new QHBoxLayout();
  hlayout->addWidget(details);
  hlayout->addWidget(btn_box, 1);

  layout->addLayout(hlayout, 10, 0, 1, 1);

  setLayout(layout);

  // select the hostname text
  cb_hostname.lineEdit()->selectAll();
  cb_hostname.lineEdit()->setFocus();
}

void GuiCompactSettingsWindow::on_close_clicked() {
  emit signal_on_close();
  this->close();
  this->deleteLater();
}

void GuiCompactSettingsWindow::on_open_clicked() {
  if (cb_hostname.currentText().isEmpty() &&
      cb_session_list.currentText() == QUTTY_DEFAULT_CONFIG_SETTINGS)
    return;

  QString configName = cb_session_list.currentText();
  if (qutty_config.config_list.find(configName) == qutty_config.config_list.end()) return;
  Conf *cfg = qutty_config.config_list[configName].get();
  conf_set_str(cfg, CONF_host, cb_hostname.currentText().toUtf8());

  conf_set_int(cfg, CONF_protocol, getConnectionType());

  chkUnsupportedConfigs(cfg);

  emit signal_on_open(cfg, openMode);
  this->close();
  this->deleteLater();
}

void GuiCompactSettingsWindow::on_details_clicked() {
  QString configName = cb_session_list.currentText();
  if (qutty_config.config_list.find(configName) == qutty_config.config_list.end()) return;
  Conf *cfg = qutty_config.config_list[configName].get();
  emit signal_on_detail(cfg, openMode);
  this->close();
  this->deleteLater();
}

void GuiCompactSettingsWindow::on_cb_session_list_activated(int n) {
  QString configName = cb_session_list.currentText();
  auto it = qutty_config.config_list.find(configName);

  if (it != qutty_config.config_list.end()) {
    Conf *cfg = it->second.get();
    char *host = conf_get_str(cfg, CONF_host);
    if (host && host[0] != '\0') hostname_completer->setText(QString(host));
    setConnectionType(conf_get_int(cfg, CONF_protocol));
  }
}

void GuiCompactSettingsWindow::on_cb_hostname_activated(const QString &str) {
  on_hostname_completion_activated(str);
  hostname_completer->popup()->hide();
}

void GuiCompactSettingsWindow::on_hostname_completion_activated(const QString &str) {
  QStringList split = str.split('|');
  if (split.length() > 1) {
    hostname_completer->setText(split[0]);

    /*
     * Based on the suggestion/technique from below url:
     * http://www.qtcentre.org/threads/14699-QCombobox-with-QTreeView-QTreeWidget
     */
    bool old = QApplication::isEffectEnabled(Qt::UI_AnimateCombo);
    if (old) QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);

    QString fullsessname = split[1];
    QAbstractItemView *treeview = cb_session_list.view();
    QModelIndex m_index = session_list_model->findIndexForSessionName(fullsessname);
    treeview->setCurrentIndex(m_index.parent());
    cb_session_list.setRootModelIndex(treeview->currentIndex());
    cb_session_list.setCurrentIndex(m_index.row());
    treeview->setCurrentIndex(QModelIndex());
    cb_session_list.setRootModelIndex(treeview->currentIndex());

    cb_session_list.showPopup();
    cb_session_list.hidePopup();
    if (old) QApplication::setEffectEnabled(Qt::UI_AnimateCombo, true);
  }
}

void GuiCompactSettingsWindow::setConnectionType(int conntype) {
  if (conntype == PROT_TELNET)
    cb_connection_type.setCurrentIndex(0);
  else
    cb_connection_type.setCurrentIndex(1);
}

int GuiCompactSettingsWindow::getConnectionType() {
  if (cb_connection_type.currentIndex() == 0) return PROT_TELNET;
  return PROT_SSH;
}
