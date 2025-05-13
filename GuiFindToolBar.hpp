/*
 * Copyright (C) 2013 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef GUIFINDTOOLBAR_H
#define GUIFINDTOOLBAR_H

#include <QLineEdit>
#include <QToolBar>

class GuiMainWindow;

class GuiFindToolBar : public QToolBar {
  Q_OBJECT

  GuiMainWindow *mainWnd = nullptr;
  QLineEdit *searchedText = nullptr;

 public:
  bool findTextFlag = false;
  int currentRow = -1;
  int currentCol = -1;
  int pageStartPosition = 0;
  QString currentSearchedText;
  explicit GuiFindToolBar(GuiMainWindow *p);
  QString getSearchedText();
  bool eventFilter(QObject *obj, QEvent *event) override;

 public slots:
  void on_findUp();
  void on_findDown();
  void on_findClose();
};

#endif  // GUIFINDTOOLBAR_H
