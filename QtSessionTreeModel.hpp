#ifndef QTSESSIONTREEMODEL_H
#define QTSESSIONTREEMODEL_H

#include <QAbstractItemModel>
#include <QStyledItemDelegate>

#include "QtConfig.hpp"
#include "QtSessionTreeItem.hpp"

class QtSessionTreeModel : public QAbstractItemModel {
  Q_OBJECT
 public:
  explicit QtSessionTreeModel(QObject *parent, std::map<QString, Config> &config_list);
  ~QtSessionTreeModel() override;

  QVariant data(const QModelIndex &index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex &index) const override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;

  QModelIndex findIndexForSessionName(QString fullsessname) const;

 private:
  QtSessionTreeItem *rootItem = nullptr;
};

class QtSessionTreeItemDelegate : public QStyledItemDelegate {
  QString displayText(const QVariant &value, const QLocale & /*locale*/) const {
    QString s = value.toString();
    int i = s.lastIndexOf('/');
    if (i != -1) return s.right(s.length() - i - 1);
    return s;
  }
};

#endif  // QTSESSIONTREEMODEL_H
