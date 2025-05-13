#ifndef QTSESSIONTREEITEM_H
#define QTSESSIONTREEITEM_H

#include <QList>
#include <QString>

class QtSessionTreeItem {
  QtSessionTreeItem(const QString &sessionName, QtSessionTreeItem *parent)
      : sessionName(sessionName), parentItem(parent) {}

  friend struct QtPrivate::QGenericArrayOps<QtSessionTreeItem>;

 public:
  explicit QtSessionTreeItem(const QString &sessionName) : sessionName(sessionName) {}

  QtSessionTreeItem &emplaceChild(const QString &name) {
    return childItems.emplace_back(name, this);
  }

  const QtSessionTreeItem *child(int row) const { return &childItems.at(row); }
  QtSessionTreeItem *child(int row) { return &childItems[row]; }

  int childCount() const { return childItems.count(); }

  int row() const {
    if (parentItem) {
      int i = 0;
      for (const QtSessionTreeItem &item : std::as_const(parentItem->childItems)) {
        if (&item == this) return i;
        i++;
      }
    }
    return 0;  // TODO: shouldn't that be -1?
  }

  QtSessionTreeItem *parent() const { return parentItem; }

  const QString &getSessionName() const { return sessionName; }

  QString getFullSessionName() const {
    if (parentItem) {
      if (parentItem->parent()) return parentItem->getFullSessionName() + "/" + sessionName;
      return sessionName;
    }
    return {};
  }

 private:
  QList<QtSessionTreeItem> childItems;
  QString sessionName;
  QtSessionTreeItem *parentItem = nullptr;
};

#endif  // QTSESSIONTREEITEM_H
