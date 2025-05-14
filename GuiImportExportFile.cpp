#include <QDialogButtonBox>
#include "GuiImportExportFile.hpp"
#include "GuiSettingsWindow.hpp"
#include "GuiMainWindow.hpp"

using std::map;

GuiImportExportFile::GuiImportExportFile(GuiMainWindow *parent, QFile *file, bool isImportMode)
    : QDialog(parent) {
  content = new QListWidget;

  QDialogButtonBox *btn_box = new QDialogButtonBox(QDialogButtonBox::Cancel);

  if (isImportMode) {
    btn_box->addButton(tr("import"), QDialogButtonBox::AcceptRole);
    connect(btn_box, SIGNAL(accepted()), this, SLOT(on_import_clicked()));

    if (file != NULL) {
      config.importFromFile(file);
      setSessions();
      setWindowTitle(tr("Import from file"));
    } else {
      config.importFromPutty();
      setSessions();
      setWindowTitle(tr("Import sessions from putty"));
    }
  } else {
    btn_box->addButton(tr("Export"), QDialogButtonBox::AcceptRole);
    connect(btn_box, SIGNAL(accepted()), this, SLOT(on_export_clicked()));

    getSessionsFromQutty();
    setWindowTitle(tr("Export sessions to file"));
  }
  connect(btn_box, SIGNAL(rejected()), this, SLOT(on_close_clicked()));
  connect(this, SIGNAL(rejected()), this, SLOT(on_close_clicked()));

  QGridLayout *layout = new QGridLayout;
  layout->addWidget(content);
  layout->addWidget(btn_box);
  setLayout(layout);
}

void GuiImportExportFile::setSessions(void) {
  Conf *cfg;

  for (map<QString, Conf *>::iterator it = config.config_list.begin();
       it != config.config_list.end(); it++) {
    QListWidgetItem *item = new QListWidgetItem(content);
    cfg = it->second;
    QString config_name = QString(conf_get_str(cfg, CONF_config_name));
    if (qutty_config.config_list.find(config_name) != qutty_config.config_list.end()) {
      item->setData(Qt::UserRole + 1, config_name);
      item->setText(config_name + tr(" (will be replaced)"));
      item->setCheckState(Qt::Unchecked);
    } else {
      item->setData(Qt::UserRole + 1, config_name);
      item->setText(config_name);
      item->setCheckState(Qt::Checked);
    }
  }
}

void GuiImportExportFile::getSessionsFromQutty(void) {
  Conf *cfg;
  for (map<QString, Conf *>::iterator it = qutty_config.config_list.begin();
       it != qutty_config.config_list.end(); it++) {
    QListWidgetItem *item = new QListWidgetItem(content);
    cfg = it->second;
    QString config_name = QString(conf_get_str(cfg, CONF_config_name));
    item->setData(Qt::UserRole + 1, config_name);
    item->setText(config_name);
    item->setCheckState(Qt::Checked);
  }
}

void GuiImportExportFile::on_close_clicked() {
  this->close();
  this->deleteLater();
}

void GuiImportExportFile::on_import_clicked() {
  int i = 0;
  Conf *cfg;
  for (i = 0; i < content->count(); i++) {
    if (content->item(i)->checkState() == Qt::Checked) {
      QString str = content->item(i)->data(Qt::UserRole + 1).toString();
      map<QString, Conf *>::iterator it = config.config_list.find(str);
      if (it != config.config_list.end()) {
        cfg = it->second;
        QString config_name = QString(conf_get_str(cfg, CONF_config_name));
        qutty_config.config_list[config_name] = cfg;
      }
    }
  }
  qutty_config.saveConfig();

  this->close();
  this->deleteLater();
}

void GuiImportExportFile::on_export_clicked() {
  int i = 0;
  Conf *cfg;
  for (i = 0; i < content->count(); i++) {
    if (content->item(i)->checkState() == Qt::Checked) {
      QString str = content->item(i)->data(Qt::UserRole + 1).toString();
      map<QString, Conf *>::iterator it = qutty_config.config_list.find(str);
      cfg = it->second;
      QString config_name = QString(conf_get_str(cfg, CONF_config_name));
      config.config_list[config_name] = cfg;
    }
  }
  QString fileName =
      QFileDialog::getSaveFileName(this, tr("Save File"), NULL, tr("XML files (*.xml)"));
  if (! fileName.isNull()) {
    QFile file(fileName);
    config.exportToFile(&file);
  }
  this->close();
  this->deleteLater();
}
