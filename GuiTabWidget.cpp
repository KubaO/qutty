/*
 * Copyright (C) 2013 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include "GuiTabWidget.hpp"

#include <QMenu>
#include <QTabBar>

#include "GuiMainWindow.hpp"
#include "GuiMenu.hpp"
#include "GuiSettingsWindow.hpp"
#include "GuiTabBar.hpp"
#include "GuiTerminalWindow.hpp"
#include "QtConfig.hpp"

GuiTabWidget::GuiTabWidget(GuiMainWindow *parent)
    : QTabWidget(parent), mainWindow(parent), guiTabBar(new GuiTabBar(this, parent)) {
  this->setTabBar(guiTabBar);
  this->setContextMenuPolicy(Qt::CustomContextMenu);
  guiTabBar->setCursor(Qt::ArrowCursor);
  connect(this, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showContextMenu(QPoint)));
}

GuiTabBar::GuiTabBar(GuiTabWidget *t, GuiMainWindow *main) : QTabBar(t), mainWindow(main) {
  setAcceptDrops(true);
}

void GuiTabWidget::showContextMenu(const QPoint &point) {
  // handling only right-click on tabbar
  if (point.isNull()) return;

  GuiTerminalWindow *termWindow;
  int menuTabIndex;
  if (((menuTabIndex = tabBar()->tabAt(point)) == -1) ||
      (!(termWindow = this->mainWindow->getCurrentTerminalInTab(menuTabIndex))))
    return;

  qutty_menu_id_t id;
  if (termWindow && !termWindow->isSockDisconnected) {
    id = MENU_RESTART_SESSION;
  } else {
    id = MENU_DUPLICATE_SESSION;
  }

  this->mainWindow->menuCookieTermWnd = termWindow;
  mainWindow->menuGetActionById(id)->setVisible(false);
  this->mainWindow->menuGetMenuById(MENU_TERM_WINDOW)->exec(this->mapToGlobal(point));
  mainWindow->menuGetActionById(id)->setVisible(true);
  this->mainWindow->menuCookieTermWnd = NULL;
}
