/*
 * Copyright (C) 2013 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef GUINAVIGATION_H
#define GUINAVIGATION_H

#include <map>
#include <stdint.h>
#include <QListWidget>

class GuiMainWindow;
class GuiTerminalWindow;

class GuiTabNavigation : public QListWidget {
  GuiMainWindow *mainWindow = nullptr;

 public:
  explicit GuiTabNavigation(GuiMainWindow *p);

  void navigateToTab(bool next);
  void acceptNavigation();

 protected:
  void focusOutEvent(QFocusEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void keyReleaseEvent(QKeyEvent *e) override;
};

class GuiPaneNavigation : public QWidget {
  GuiMainWindow *mainWindow = nullptr;
  std::map<uint32_t, GuiTerminalWindow *> mrupanemap;
  uint32_t curr_sel = -1;

 public:
  GuiPaneNavigation(GuiMainWindow *p, bool is_direction_mode = false);

  void navigateToDirectionPane(Qt::Key key);
  void navigateToMRUPane(bool next);
  void acceptNavigation();

 protected:
  void focusOutEvent(QFocusEvent *e) override;
  void keyReleaseEvent(QKeyEvent *e) override;
};

#endif  // GUINAVIGATION_H
