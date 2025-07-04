#ifndef GUIIMPORTEXPORTFILE_H
#define GUIIMPORTEXPORTFILE_H

#include <QDialog>
#include <QFile>
#include <QGridLayout>
#include <QIODevice>
#include <QListWidget>
#include <QPushButton>

#include "QtConfig.hpp"

class GuiMainWindow;

class GuiImportExportFile : public QDialog {
  Q_OBJECT
 private:
  QListWidget *content = nullptr;
  QtConfig config = {};

  void setSessions(void);
  void getSessionsFromQutty(void);

 public:
  GuiImportExportFile(GuiMainWindow *parent, QFile *file, bool isImportMode = false);

 public slots:
  void on_import_clicked();
  void on_export_clicked();
  void on_close_clicked();
};

#endif  // GUIIMPORTEXPORTFILE_H
