#ifndef GUICOMPACTSETTINGSWINDOW_H
#define GUICOMPACTSETTINGSWINDOW_H

#include <QComboBox>
#include <QDialog>
#include <QPainter>

#include "GuiBase.hpp"
#include "QtComboBoxWithTreeView.hpp"
#include "QtSessionTreeModel.hpp"

class GuiMainWindow;
class QtCompleterWithAdvancedCompletion;

class GuiCompactSettingsWindow : public QDialog {
  Q_OBJECT

  QtComboBoxWithTreeView cb_session_list;
  QComboBox cb_connection_type;
  QComboBox cb_hostname;
  GuiBase::SplitType openMode = {};
  QtCompleterWithAdvancedCompletion *hostname_completer;

  QtSessionTreeModel *session_list_model = nullptr;

  void setConnectionType(int conntype);
  int getConnectionType();

 public:
  explicit GuiCompactSettingsWindow(QWidget *parent,
                                    GuiBase::SplitType openmode = GuiBase::TYPE_LEAF);

 signals:
  void signal_on_open(const PuttyConfig *cfg, GuiBase::SplitType splittype);
  void signal_on_close();
  void signal_on_detail(const PuttyConfig *cfg, GuiBase::SplitType splittype);

 public slots:
  void on_open_clicked();
  void on_close_clicked();
  void on_details_clicked();
  void on_cb_session_list_activated(int);
  void on_cb_hostname_activated(const QString &);
  void on_hostname_completion_activated(const QString &);
};

class QtHostNameCompleterItemDelegate : public QStyledItemDelegate {
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStringList split = index.model()->data(index).toString().split('|');
    if (split.length() != 2) return;

    if (option.state & QStyle::State_Selected)
      painter->fillRect(option.rect, option.palette.highlight());
    const QString &hostname = split[0];
    const QString &sessname = split[1];
    painter->drawText(option.rect, hostname);
    painter->drawText(option.rect, Qt::AlignRight, sessname);
  }
};

#endif  // GUICOMPACTSETTINGSWINDOW_H
