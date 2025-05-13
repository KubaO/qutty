#ifndef QTMRUSESSIONLIST_H
#define QTMRUSESSIONLIST_H

#include <QVector>
#include <QPair>
#include <QString>

class QtMRUSessionList {
 public:
  void initialize();
  void insertSession(QStringView sessname, QStringView hostname);
  void deleteSession(QStringView sessname);
  QVector<QPair<QString, QString> > mru_list;

 private:
  void save();
};

extern QtMRUSessionList qutty_mru_sesslist;

#endif  // QTMRUSESSIONLIST_H
