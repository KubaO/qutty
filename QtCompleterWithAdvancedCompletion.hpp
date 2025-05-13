#ifndef QTCOMPLETERWITHADVANCEDCOMPLETION_H
#define QTCOMPLETERWITHADVANCEDCOMPLETION_H

#include <QCompleter>
#include <QListView>
#include <QStringListModel>
#include <QLineEdit>
#include <QComboBox>

class QtCompleterWithAdvancedCompletion : public QObject {
  Q_OBJECT

 public:
  enum FilterMode { StartsWith, Contains, ContainsWord };

 private:
  QWidget *w = nullptr;
  QListView *popuplist = nullptr;
  QStringList completions;
  QStringListModel *model = nullptr;
  int maxVisibleItems = 10;
  int noItemsShown = 0;
  FilterMode filterMode = QtCompleterWithAdvancedCompletion::ContainsWord;
  bool is_keypress = false;
  bool always_show_popup = false;

  void init();

 public:
  explicit QtCompleterWithAdvancedCompletion(QLineEdit *le);
  explicit QtCompleterWithAdvancedCompletion(QComboBox *cb);
  ~QtCompleterWithAdvancedCompletion() override;

  void setModel(QStringList &completions);
  void setMaxVisibleItems(int maxItems) { maxVisibleItems = maxItems; }
  void setFilterMode(FilterMode mode) { filterMode = mode; }

  QListView *popup() const { return popuplist; }

  void set_always_show_popup(bool b) { always_show_popup = b; }

 protected:
  bool eventFilter(QObject *o, QEvent *e) override;

 signals:
  void activated(const QString &text);
  void deactivated();

 private slots:
  void slot_completerComplete(QModelIndex index);

 public slots:
  void completionSearchString(QString str);
  void setText(QString str);
};

#endif  // QTCOMPLETERWITHADVANCEDCOMPLETION_H
