#ifndef GUICOMPACTSETTINGSWINDOW_H
#define GUICOMPACTSETTINGSWINDOW_H

#include <QDialog>
#include <QStyledItemDelegate>

#include "GuiBase.hpp"
extern "C" {
#include "defs.h"
}
class GuiMainWindow;
class QCompleter;
class QtSessionTreeModel;

namespace Ui {
class GuiCompactSettingsWindow;
}

class GuiCompactSettingsWindow : public QDialog {
  Q_OBJECT

  GuiBase::SplitType openMode = {};
  QCompleter *hostname_completer = nullptr;
  QtSessionTreeModel *session_list_model = nullptr;

  void setConnectionType(int conntype);
  int getConnectionType();

 public:
  explicit GuiCompactSettingsWindow(QWidget *parent,
                                    GuiBase::SplitType openmode = GuiBase::TYPE_LEAF);

 signals:
  void signal_on_open(Conf *cfg, GuiBase::SplitType splittype);
  void signal_on_close();
  void signal_on_detail(Conf *cfg, GuiBase::SplitType splittype);

 public slots:
  void on_bb_buttons_accepted();
  void on_bb_buttons_rejected();
  void on_btn_details_clicked();
  void on_cb_session_list_activated(int);
  void on_cb_hostname_textActivated(const QString &);
  void hostname_completion_activated(const QString &);

 private:
  Ui::GuiCompactSettingsWindow *ui;
};

#endif  // GUICOMPACTSETTINGSWINDOW_H
