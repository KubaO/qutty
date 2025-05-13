#ifndef GUITEXTFILTERWINDOW_H
#define GUITEXTFILTERWINDOW_H

#include <QToolBar>
#include <QLineEdit>
#include <QPlainTextEdit>

#include "GuiMainWindow.hpp"
#include "QtCompleterWithAdvancedCompletion.hpp"

class GuiTextFilterWindow : public QToolBar {
  Q_OBJECT

  GuiMainWindow *mainWnd = nullptr;
  QLineEdit *filter = nullptr;
  QtCompleterWithAdvancedCompletion *completer = nullptr;
  QToolButton *edit = nullptr, *save = nullptr;
  QPlainTextEdit *editor = nullptr;
  QString label;
  bool is_editable = false;

  QStringList getCompletions();
  void setCompletions(QString str);

 public:
  explicit GuiTextFilterWindow(GuiMainWindow *p, bool isEditable, QString lbl);

  void init();

  QSize sizeHint() const override;

 public slots:
  void on_text_completion_activated(QString str);
  void on_deactivated();
  void on_editList();
  void on_saveList();
};

#endif  // GUITEXTFILTERWINDOW_H
