/*
 * Copyright (C) 2013 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef GUILAYOUT_H
#define GUILAYOUT_H
#include <QSplitter>
#include <vector>

class GuiTerminalWindow;
class GuiSplitter;

class GuiBase {
 public:
  enum SplitType {
    TYPE_NONE = -1,
    TYPE_UP = 0,
    TYPE_LEFT = 1,
    TYPE_DOWN = 2,
    TYPE_RIGHT = 3,
    TYPE_LEAF = 4,
    TYPE_HORIZONTAL = TYPE_DOWN,
    TYPE_VERTICAL = TYPE_RIGHT
  };
  GuiSplitter *parentSplit;

  GuiBase();
  virtual ~GuiBase(){};
  virtual void reqCloseTerminal(bool userRequest) = 0;
  virtual QWidget *getWidget() = 0;
  virtual void populateAllTerminals(std::vector<GuiTerminalWindow *> *list) = 0;
};

// http://stackoverflow.com/questions/3726716/qt-interfaces-or-abstract-classes-and-qobject-cast
Q_DECLARE_INTERFACE(GuiBase, "QuTTY/GuiBase/1.0");

#endif  // GUILAYOUT_H
