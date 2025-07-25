/*
 * Copyright (C) 2013 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef GUIDRAG_H
#define GUIDRAG_H

#include <QGridLayout>
#include <QToolButton>

#include "GuiBase.hpp"

class GuiBase;
class GuiTabBar;

class GuiDragDropSite : public QWidget {
 public:
  enum GuiDragDropLocation { DRAG_DROP_NONE, DRAG_DROP_ON_TERMINAL, DRAG_DROP_ON_TABBAR };

 private:
  QGridLayout layout;
  GuiDragDropLocation drop_loc = GuiDragDropSite::DRAG_DROP_NONE;
  int tabind = -1;
  QToolButton btn[5];

 public:
  GuiBase::SplitType drop_mode = GuiBase::TYPE_NONE;

  explicit GuiDragDropSite(QWidget *parent = nullptr);
  GuiBase::SplitType updateDropMode(const QPoint &pos);
  int updateDropOnTabBar(const QPoint &pos, GuiTabBar *tab);
  void clearDropMode();
  void paintEvent(QPaintEvent *e) override;

  GuiBase::SplitType get_drop_mode() { return drop_mode; }
  int get_tabind() { return tabind; }
  GuiDragDropLocation get_drop_loc() { return drop_loc; }
};

#endif  // GUIDRAG_H
