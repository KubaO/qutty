/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenu>
#include <QShortcut>
#include <QSignalMapper>
#include <QToolButton>

#include "GuiCompactSettingsWindow.hpp"
#include "GuiDrag.hpp"
#include "GuiMenu.hpp"
#include "GuiNavigation.hpp"
#include "GuiSettingsWindow.hpp"
#include "QtCommon.hpp"

class GuiCompactSettingsWindow;
class GuiSettingsWindow;
class GuiFindToolBar;
class GuiTabWidget;
class GuiTextFilterWindow;

class GuiToolButton : public QToolButton {
  Q_OBJECT

 protected:
  void paintEvent(QPaintEvent *) override;

 public:
  QSize sizeHint() const override;
};

class GuiMainWindow : public QMainWindow {
  Q_OBJECT

 public:
  GuiTerminalWindow *menuCookieTermWnd = nullptr;
  GuiToolbarTerminalTop toolBarTerminalTop;  // top-right of terminal in split-mode

  // members for drag-drop support
  GuiDragDropSite dragDropSite;

  // find window
  GuiFindToolBar *findToolBar = nullptr;

  // tab order-of-usage navigation window
  uint32_t mru_count_last = 0;
  GuiTabNavigation *tabNavigate = nullptr;
  GuiPaneNavigation *paneNavigate = nullptr;

  GuiTabWidget *tabArea = nullptr;

  // text filter window.
  GuiTextFilterWindow *textFilterWnd = nullptr;

 private:
  GuiCompactSettingsWindow *compactSettingsWindow = nullptr;
  GuiSettingsWindow *settingsWindow = nullptr;
  QList<GuiTerminalWindow *> terminalList;
  GuiToolButton newTabToolButton;  // shown in top right corner of tabbar

  // containers for fast-retrieval
  std::map<const QWidget *, int> tabIndexMap;
  std::vector<std::pair<GuiSplitter *, GuiTerminalWindow *>> widgetAtIndex;

  // members for action/menu support
  std::vector<std::tuple<int32_t, QShortcut *, QAction *>> menuCommonShortcuts;
  QMenu menuCommonMenus[MENU_MAX_MENU];
  QMenu menuSavedSessions;
  QSignalMapper *menuCustomSavedSessionSigMapper;

 public:
  explicit GuiMainWindow(QWidget *parent = 0);
  ~GuiMainWindow() override;
  void createNewTab(const PuttyConfig *cfg, GuiBase::SplitType splittype = GuiBase::TYPE_LEAF);
  void closeEvent(QCloseEvent *event) override;
  GuiTerminalWindow *getCurrentTerminal();
  GuiTerminalWindow *getCurrentTerminalInTab(int tabIndex);

  QMenu *menuGetMenuById(qutty_menu_id_t id) {
    assert(id > MENU_SEPARATOR && id <= MENU_SEPARATOR + MENU_MAX_MENU);
    return &menuCommonMenus[id - MENU_SEPARATOR - 1];
  }
  QAction *menuGetActionById(qutty_menu_id_t id);
  QKeySequence menuGetShortcutById(qutty_menu_id_t id);
  void menuSetShortcutById(qutty_menu_id_t id, const QKeySequence &key);
  void initializeCustomSavedSessionShortcuts();

  void tabInsert(int tabind, QWidget *w, const QString &title);
  void tabRemove(int tabind);
  int setupLayout(GuiTerminalWindow *newTerm, GuiSplitter *splitter);
  int setupLayout(GuiTerminalWindow *newTerm, GuiBase::SplitType split, int tabind = -1);
  void setupTerminalSize(GuiTerminalWindow *newTerm);

  int getTerminalTabInd(const QWidget *term);

  const QList<GuiTerminalWindow *> *getTerminalList() { return &terminalList; }

 private:
  void initializeMenuSystem();
  void inittializeDragDropWidget();
  void populateMenu(QMenu &menu, qutty_menu_id_t menu_list[], int len);

  void readWindowSettings();
  void writeWindowSettings();

 public slots:
  void on_openNewWindow();
  void on_openNewCompactSession(GuiBase::SplitType splittype);
  void on_openNewSession(const PuttyConfig *cfg, GuiBase::SplitType splittype);
  void on_openNewTab() { on_openNewCompactSession(GuiBase::TYPE_LEAF); }
  void on_openNewSplitHorizontal() { on_openNewCompactSession(GuiBase::TYPE_HORIZONTAL); }
  void on_openNewSplitVertical() { on_openNewCompactSession(GuiBase::TYPE_VERTICAL); }
  void on_createNewSession(const PuttyConfig *cfg, GuiBase::SplitType splittype);
  void on_settingsWindowClose();
  void on_changeSettingsTab(GuiTerminalWindow *termWnd);
  void on_changeSettingsTabComplete(const PuttyConfig *cfg, GuiTerminalWindow *termWnd);
  void closeTerminal(GuiTerminalWindow *termWnd);
  void hideTerminal(GuiTerminalWindow *termWnd);
  void tabCloseRequested(int index);
  void currentChanged(int index);
  void on_tabLayoutChanged();

  void tabNext();
  void tabPrev();

  void contextMenuPaste();
  void contextMenuSavedSessionsChanged();
  void contextMenuSavedSessionTriggered();
  void contextMenuDuplicateSessionTriggered();
  void contextMenuOpenDuplicateHSplit();
  void contextMenuOpenDuplicateVSplit();
  void contextMenuRestartSessionTriggered();
  void contextMenuChangeSettingsTriggered();
  void contextMenuCloseSessionTriggered();
  void contextMenuCloseWindowTriggered();
  void contextMenuMenuBar();
  void contextMenuTabInTitleBar();
  void contextMenuFullScreen();
  void contextMenuAlwaysOnTop();
  void contextMenuTermTopDragPaneTriggered();
  void contextMenuTermTopCloseTriggered();
  void contextMenuPreferences();
  void contextMenuRenameTab();
  void contextMenuFind();
  void contextMenuFindNext();
  void contextMenuFindPrevious();
  void contextMenuPaneUp();
  void contextMenuPaneDown();
  void contextMenuPaneLeft();
  void contextMenuPaneRight();
  void contextMenuMRUTab();
  void contextMenuLRUTab();
  void contextMenuMRUPane();
  void contextMenuLRUPane();
  void contextMenuCustomSavedSession(int ind);
  void contextMenuImportFromFile();
  void contextMenuImportFromPuttySessions();
  void contextMenuExportToFile();
  void contextMenuAutoComplete();
  void contextMenuPasteHistory();
};

#endif  // MAINWINDOW_H
