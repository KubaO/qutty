#include "GuiCompactSettingsWindow.hpp"

#include <QCompleter>
#include <QLineEdit>
#include <QPainter>
#include <QStringList>
#include <QStringListModel>
#include <QTreeView>

#include "GuiMainWindow.hpp"
#include "QtSessionTreeModel.hpp"
#include "serialize/QtMRUSessionList.hpp"
#include "ui_GuiCompactSettingsWindow.h"

using std::map;

class QtHostNameCompleterItemDelegate : public QStyledItemDelegate {
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
};

GuiCompactSettingsWindow::GuiCompactSettingsWindow(QWidget *parent, GuiBase::SplitType openmode)
    : QDialog(parent), ui(new Ui::GuiCompactSettingsWindow) {
  ui->setupUi(this);
  setModal(true);
  openMode = openmode;

  connect(this, &QDialog::rejected, this, &GuiCompactSettingsWindow::on_bb_buttons_rejected);

  ui->cb_hostname->setMinimumWidth(500);

  QStringList completions;
  for (auto it = qutty_mru_sesslist.mru_list.begin(); it != qutty_mru_sesslist.mru_list.end();
       ++it) {
    if (it->second.isEmpty() || it->second[0] == QChar('\0')) continue;
    // in 'hostname|sessname' format
    completions << it->second + "|" + it->first;
  }
  QStringListModel *hostCompletions = new QStringListModel(completions, this);
  hostname_completer = new QCompleter(ui->cb_hostname);
  hostname_completer->setModel(hostCompletions);
  hostname_completer->popup()->setItemDelegate(new QtHostNameCompleterItemDelegate);
  connect(hostname_completer, SIGNAL(activated(QString)), this,
          SLOT(on_hostname_completion_activated(QString)));
  ui->cb_hostname->addItems(completions);
  ui->cb_hostname->setItemDelegate(new QtHostNameCompleterItemDelegate);

  QTreeView *view = new QTreeView(this);
  view->setHeaderHidden(true);
  ui->cb_session_list->setView(view);
  ui->cb_session_list->setItemDelegate(new QtSessionTreeItemDelegate);
  session_list_model = new QtSessionTreeModel(this, qutty_config.config_list);
  ui->cb_session_list->setModel(session_list_model);
  ui->cb_session_list->setMaxVisibleItems(25);

  ui->cb_connection_type->addItem("Telnet", PROT_TELNET);
  ui->cb_connection_type->addItem("SSH", PROT_SSH);

  if (qutty_mru_sesslist.mru_list.size() > 0 &&
      qutty_config.config_list.find(qutty_mru_sesslist.mru_list[0].first) !=
          qutty_config.config_list.end() &&
      !completions.isEmpty()) {
    QString sessname = qutty_mru_sesslist.mru_list[0].first;
    hostname_completion_activated(completions.at(0));
    setConnectionType(conf_get_int(qutty_config.config_list[sessname].get(), CONF_protocol));
  } else if (qutty_config.config_list.find(QUTTY_DEFAULT_CONFIG_SETTINGS) !=
             qutty_config.config_list.end()) {
    Conf *cfg = qutty_config.config_list[QUTTY_DEFAULT_CONFIG_SETTINGS].get();
    ui->cb_session_list->setCurrentIndex(
        ui->cb_session_list->findText(QUTTY_DEFAULT_CONFIG_SETTINGS));
    ui->cb_hostname->setEditText(conf_get_str(cfg, CONF_host));
    setConnectionType(conf_get_int(cfg, CONF_protocol));
  }

  connect(ui->cb_session_list, &QComboBox::activated, this,
          &GuiCompactSettingsWindow::on_cb_session_list_activated);

  // select the hostname text
  ui->cb_hostname->lineEdit()->selectAll();
  ui->cb_hostname->lineEdit()->setFocus();
}

void GuiCompactSettingsWindow::on_bb_buttons_rejected() {
  emit signal_on_close();
  this->close();
  this->deleteLater();
}

void GuiCompactSettingsWindow::on_bb_buttons_accepted() {
  if (ui->cb_hostname->currentText().isEmpty() &&
      ui->cb_session_list->currentText() == QUTTY_DEFAULT_CONFIG_SETTINGS)
    return;

  QString configName = ui->cb_session_list->currentText();
  if (qutty_config.config_list.find(configName) == qutty_config.config_list.end()) return;
  Conf *cfg = qutty_config.config_list[configName].get();
  conf_set_str(cfg, CONF_host, ui->cb_hostname->currentText().toUtf8());

  conf_set_int(cfg, CONF_protocol, getConnectionType());

  chkUnsupportedConfigs(cfg);

  emit signal_on_open(cfg, openMode);
  this->close();
  this->deleteLater();
}

void GuiCompactSettingsWindow::on_btn_details_clicked() {
  QString configName = ui->cb_session_list->currentText();
  if (qutty_config.config_list.find(configName) == qutty_config.config_list.end()) return;
  Conf *cfg = qutty_config.config_list[configName].get();
  emit signal_on_detail(cfg, openMode);
  this->close();
  this->deleteLater();
}

void GuiCompactSettingsWindow::on_cb_session_list_activated(int n) {
  QString configName = ui->cb_session_list->currentText();
  auto it = qutty_config.config_list.find(configName);

  if (it != qutty_config.config_list.end()) {
    qDebug() << "**";
    Conf *cfg = it->second.get();
    char *host = conf_get_str(cfg, CONF_host);
    if (host && host[0] != '\0') ui->cb_hostname->setEditText(QString(host));
    setConnectionType(conf_get_int(cfg, CONF_protocol));
  }
}

void GuiCompactSettingsWindow::on_cb_hostname_textActivated(const QString &str) {
  hostname_completion_activated(str);
  hostname_completer->popup()->hide();
}

void GuiCompactSettingsWindow::hostname_completion_activated(const QString &str) {
  QStringList split = str.split('|');
  if (split.length() > 1) {
    ui->cb_hostname->setEditText(split[0]);

    /*
     * Based on the suggestion/technique from below url:
     * http://www.qtcentre.org/threads/14699-QCombobox-with-QTreeView-QTreeWidget
     */
    bool old = QApplication::isEffectEnabled(Qt::UI_AnimateCombo);
    if (old) QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);

    QString fullsessname = split[1];
    QAbstractItemView *treeview = ui->cb_session_list->view();
    QModelIndex m_index = session_list_model->findIndexForSessionName(fullsessname);
    treeview->setCurrentIndex(m_index.parent());
    ui->cb_session_list->setRootModelIndex(treeview->currentIndex());
    ui->cb_session_list->setCurrentIndex(m_index.row());
    treeview->setCurrentIndex(QModelIndex());
    ui->cb_session_list->setRootModelIndex(treeview->currentIndex());

    ui->cb_session_list->showPopup();
    ui->cb_session_list->hidePopup();
    if (old) QApplication::setEffectEnabled(Qt::UI_AnimateCombo, true);
  }
}

void GuiCompactSettingsWindow::setConnectionType(int conntype) {
  qDebug() << __FUNCTION__ << conntype;
  for (int i = 0; i < ui->cb_connection_type->count(); i++) {
    bool ok;
    int data = ui->cb_connection_type->itemData(i).toInt(&ok);
    if (ok && data == conntype) {
      ui->cb_connection_type->setCurrentIndex(i);
      break;
    }
  }
}

int GuiCompactSettingsWindow::getConnectionType() {
  bool ok;
  int data = ui->cb_connection_type->currentData().toInt(&ok);
  qDebug() << "->" << (ok ? data : -1);
  return ok ? data : -1;
}

void QtHostNameCompleterItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const {
  QStringList split = index.model()->data(index).toString().split('|');
  if (split.length() != 2) return;

  if (option.state & QStyle::State_Selected)
    painter->fillRect(option.rect, option.palette.highlight());
  const QString &hostname = split[0];
  const QString &sessname = split[1];
  painter->drawText(option.rect, hostname);
  painter->drawText(option.rect, Qt::AlignRight, sessname);
}
