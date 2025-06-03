# Features

- Multiple Tab interface.
- Multiple Split-pane interface.
- Easy navigation among Tabs/Split-panes.
- Easy drag-drop of Tabs/Split-panes.
- Configurable keyboard shortcuts.
- Organizing Sessions in tree hierarchy.
- Autocomplete support allows sessions to be selected easily.

## Multiple Tab/Split-pane interface

QuTTY supports multiple Tabs. Each Tab can be further split horizontally or vertically into multiple panes in any imaginable order.

### Requirements

- Tabs in Titlebar is only supported in Windows Vista or later. Older versions don't show Tabs in titlebar.
- This feature only works if Desktop Composition visual effect is enabled. Please see [here on how to enable](http://www.sevenforums.com/tutorials/127411-desktop-composition-enable-disable.html).
- Download the QuTTY executable depending on whether you have 32-bit or 64-bit Windows.

### Details

The tabs are shown in titlebar similar to Google Chrome. Let us know your feedback on [QuTTY discuss](http://groups.google.com/group/qutty-discuss). Don't forget to report any issues in [issue-tracker](http://code.google.com/p/qutty/issues/list)

![http://wiki.qutty.googlecode.com/git/img/tabs-in-titlebar.png](http://wiki.qutty.googlecode.com/git/img/tabs-in-titlebar.png)

### Enabling/Disabling feature

The feature will be automatically enabled upon first-run of QuTTY, if the aforementioned requirements are met. You can enable/disable at will from the menu 'View -> Show Tabs in Titlebar' checkbox. Just make sure to restart the application for this change to take into effect.

## Easy drag-drop of Tabs/Split-panes

Tabs can be easily rearranged by drag-dropping in tab Bar. Split-panes can be rearranged by Ctrl + Rightclick and then drag and drop to a desired terminal.

## Configurable keyboard shortcuts

The user can further configure shortcuts for many different actions.

Following are the default keyboard shortcuts supported in QuTTY. The default shortcuts are influenced by shortcuts in [Konsole](https://konsole.kde.org/) and [iTerm2](https://www.iterm2.com/). All the shortcuts can be remapped from 'Preferences' window.

### Create new tab/split-pane

- Ctrl + Shift + t to create a new Tab.
- Ctrl + Shift + h to create a horizontal split-pane.
- Ctrl + Shift + v to create a vertical split-pane.

### Navigation between tabs/split-panes

- Shift + Left/Right Arrow navigates between tabs.
- Ctrl + Shift + Up/Down/Left/Right navigates between split-panes.
- Ctrl + Tab and Ctrl + Shift + Tab navigate between tabs in order of use.
- Ctrl + Shift + \[ and Ctrl + Shift + ] navigate between split-panes in order of use.

### Search

- Ctrl + Shift + F to bring up the find window. **TODO**
- F3 and Shift + F3 to find next and previous match respectively. **TODO**

## Session Tree Hierarchy

Sessions can be organized in a tree hierarchy. This helps if you are in need of managing quite a large number of sessions. Just drag and drop sessions on top of each other to create the hierarchy.

## New Compact Settings Window

Instead of the PuTTY style classical settings window with detailed configuration, a light-weight compact settings window is introduced with minimal configuration.


## Tmux Integration

### Introduction

See https://iterm2.com/documentation-tmux-integration.html

### Building Tmux

First get the tmux2 repository's 'command_mode' branch.

    git clone https://github.com/gnachman/tmux2.git
    git checkout -t origin/command_mode

Now build as follows: 
    cd tmux2
    sh autogen.sh
    ./configure
    make

# How To Build QuTTY

## Prerequisites

- Qt 4.8.0 or Qt 4.8.4 (Qt 5.0 is not yet supported)
- Visual Studio Express 2010 or 2012 or MingW gcc
- git to download code

## Download code

- `git clone https://code.google.com/p/qutty/`

## Compiling

Visual Studio Express or Mingw gcc compilers are supported in Qt. You can either use Qt Creator GUI or use command prompt to compile.

### Using Qt Creator

Compiling using Qt Creator is straight forward. Just open the 'QuTTY.pro' file in Qt Creator. You should be able to build & run the project from 'Build' menu. Qt Creator should be already configured with one of the supported compiler.

### Using command prompt

Lets assume Qutty has been downloaded to `c:\tmp\qutty\` and Qt has been installed in `c:\Qt\qt-4.8.4\`. Note that the installed Qt should support the compiler you are planning to compile qutty with.

#### Visual Studio command prompt

Open the Visual Studio command prompt from `Start Menu -> All Programs -> Microsoft Visual Studio -> Visual Studio Tools -> Native Tools command prompt` * `cd c:\tmp\qutty` * `c:\Qt\qt-4.8.4\bin\qmake.exe -spec win32-msvc2012` * `nmake` Note that you should use `-spec win32-msvc2010` if compiling using Visual Studio Express 2010.

#### gcc command prompt

Open the command prompt. Make sure mingw bin path is included in your `PATH` environment variable. * `cd c:\tmp\qutty` * `c:\Qt\qt-4.8.4\bin\qmake.exe -spec win32-g++` * `mingw32-make`

The qutty binary will be created in `bin\QuTTY.exe`