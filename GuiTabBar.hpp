/*
 * Copyright (C) 2013 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef GUITABBAR_H
#define GUITABBAR_H

#include <QTabBar>

class GuiTabWidget;
class GuiMainWindow;

class GuiTabBar : public QTabBar {
  Q_OBJECT

  GuiMainWindow *mainWindow = nullptr;

 public:
  GuiTabBar(GuiTabWidget *t, GuiMainWindow *main);

  // Needed functions for drag-drop support
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dragLeaveEvent(QDragLeaveEvent *e) override;
  void dragMoveEvent(QDragMoveEvent *e) override;
  void dropEvent(QDropEvent *e) override;

 protected:
  void tabInserted(int index) override {
    (void)index;
    emit sig_tabInserted();
  }
  void tabRemoved(int index) override {
    (void)index;
    emit sig_tabRemoved();
  }

signals:
  void sig_tabInserted();
  void sig_tabRemoved();
};

#endif  // GUITABBAR_H
