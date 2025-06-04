# QuTTY = PuTTY + Qt

QuTTY is a front end for PuTTY written in Qt. It also adds a TMUX backend.

## But, Why?

PuTTY itself is a marvel of engineering. In spite of complexity, the code base is understandable, well factored, and has been *improving* in maintainability over the years (!). How many OSS projects born in the 90s do you know that have a modern cmake-based build system? :)

The 32-bit EXE, with a built-in online manual (!), *almost* fits on a 1.44MB floppy. A ZIP archive of it is about 850kb. It *still* works on Windows XP, and still supports legacy ssh servers running on obsolete hardware and obsolete OSes - with their numerous quirks and bug. I am sure that with a bit of further refactoring, the EXE could be made to fit on that 1.44MB floppy for the foreseeable future, even without the use of an EXE compressor. There are very few open-source projects of similar complexity that could lay similar claims.

"Porting" PuTTY to Qt does not make it inherently better. It does *not* make it smaller for sure! With care, linking Qt modules statically, and using a feature-trimmed build of Qt tailored for QuTTY, it could be a single EXE well below 10MB. Still not small, but tiny compared to dynamically linked Qt applications.

QuTTY could be a springboard for PuTTY on mobile platforms that Qt supports -- but that's not in my plans for now.

For me, the project is mostly a practical way of exploring PuTTY's source code and design.

## Goals

- use Qt for portability - use its text codecs, network module, and gui
- have Qt as the only external dependency
- feature parity with PuTTY.exe on desktop platforms supported by Qt
- no use of sources in `puttysrc/platform` - that's what Qt is for
- command-line tools bundled with PuTTY are a non-goal even though they certainly could use Qt for portability

In other words: I have no interest in making Qt 6 target Windows XP or Snow Leopard, even though technically it's feasible.

## Status

**This project is nowhere near being usable. It is in heavy development. Temporarily, only Windows are supported.**

A couple of man-weeks of went into bringing the PuTTY sources bundled with QuTTY up to the current (0.83) release of PuTTY. Qt 6 is used in place of Qt 5. Before that, QuTTY was based on PuTTY 0.62 from 13 years ago, was a mostly Qt 4 project that happened to also build on Qt 5. 

The interface between PuTTY and the backends has changed dramatically between then and now, so a lot had to change in QuTTY as well. I used this opportunity to dig deep into how PuTTY itself has changed over that period, and how it "ticks".

The porting work did not remove functionality willy-nilly, but some things are definitely hacked together at the moment, or are buggy.  The changes in QuTTY were minimal in nature just to make an SSH connection possible. Nothing past establishing a connection and keyboard interaction has been tested. At the moment, QuTTY does not support quite a few keyboard and mouse interaction features of native PuTTY, there's no bell, etc.

A bit of work on the settings GUI was done to make the layout more appealing, and to keep up with new configuration settings added to PuTTY itself. There are still some major TODOs there. It may be worthwhile to leverage PuTTY's portable configuration system as a single source of truth for the UI elements in the future.

### Changes to PuTTY Sources

The upgrade of PuTTY source code was done incrementally, usually with a single branch moving them only a point version ahead - e.g. from 0.62 to 0.63. Each upgrade aimed to minimize the differences between files in `puttysrc/`  and the upstream sources. The original QuTTY has made several changes to PuTTY that complicated this job a bit. All is good, however.

The near-term plan is to use pristine PuTTY sources. Currently, only the following sources are different from PuTTY source distribution:

`conf.h`, `network.h`, `putty.h`, `puttymem.h`, `ssh.c`, `telnet.c`,`terminal.c`, `terminal.h`, `version.c`, `version.h`.

Here is the plan to get rid of these changes:

- [x]  `conf.h` - the `config_name` option is used internally in QuTTY and does not need to be exposed in the header.
- [ ] `network.h` - a bug will be reported upstream, it's essentially a leftover of prior changes in PuTTY.
- [x] `putty.h` - the TMUX backend doesn't need to be listed in the enum within `putty.h` proper.
- [ ] `puttymem.h` - a change will be suggested upstream to make that header usable from C++.
- [x] `ssh.c` and `telnet.c` - PuTTY's architecture is such that neither the front-end nor the networking layer need direct access to platform sockets kept in the backends. It's a leftover from QuTTY.
- [ ] `terminal.c` and `terminal.h` 
    - [ ] The front end doesn't need direct access to terminal's character and attribute buffers. Some work was already done to get rid of that dependency.
    - [ ] An investigation is needed into the TMUX gateway integration. It certainly could be less intrusive.
    
- [ ] `version.c` and `version.h` - mostly gratuitous changes since we're nowhere near any sort of a public release anyway.

- [ ] `[target]/platform.h` are customized for Qt-as-a-platform and are expected to diverge significantly. The platform headers will be removed from the `puttysrc` tree since they are not meant to track upstream directly.

QuTTY doesn't leverage PuTTY's cmake build scripts. Eventually it probably should.

## Archived WIKI

See [the archived Wiki](documentation/Wiki.md).

## Previous README

QuTTY is a multi-tabbed, multi-paned SSH/Telnet client with a terminal emulator. The goal is to support advanced features similar to iterm2.

Just download and try it. If you see any issues you can try the following. If you still face issues report it to us.

- install the Visual Studio runtime [VC++ 2015].
- Configure qutty executable to be run in compatibility mode for Windows 7/Vista/Xp to see if it helps.
- Look for any coredumps in %USERPROFILE%\qutty\dumps\ and email them if any.

Report any issues or feature-requests in [issue-tracker]. We need help from volunteers to find and fix any blocking issues, so that a stable release can be made soon.

### Links to some documentation:
- [Features]
- [TabsInTitlebar]
- [HierarchicalSessions]
- [KeyboardShortcuts]
- [TmuxIntegration]
- [HowToBuild]

#### 0.8-beta release (08/21/2013)
Eighth beta release is made today with a few bugfixes. Thanks to Dragan, Nick Bisby for reporting some important issues. Thanks to Suriya priya Veluchamy for fixing them as well.

#### 0.7-beta release (07/29/2013)
Seventh beta release is made today with these features.

- Major feature supported is tabs in titlebar similar to Firefox/Chrome. See [TabsInTitlebar] for more information.
- Suriya priya Veluchamy has implemented support for logging sessions similar to Putty.
- Few other minor features & bugfixes were in as well.

#### 0.6-beta release (07/15/2013)
Sixth beta release is made today. Thanks to Suriya priya Veluchamy for implementing/testing these much-needed features. Thanks to Nick Bisby for suggesting these features as well.

- Sessions can be organized in a hierarchical session tree.
- A new compact settings window is introduced instead of the classical detailed PuTTY style window.
- Auto completion of hostname allows selecting a session/profile easily.
- Import/Export sessions is supported. This allows settings to be imported from XML file/PuTTY registry and to be exported as XML file.
- Last but not least, QuTTY icon has been updated by Suriya priya (created from scratch using gimp 2.8.4 opensource tool, instead of freeware/shareware tools used for older icon. gimp project files are available as well, so that future modifications will be easy).

Also see [HierarchicalSessions] for more documentation & tips.

#### 0.5-beta release (06/23/2013)
Fifth beta release is made today.

- Main feature added is configurable keyboard shortcuts.
- Navigation among Tabs/Split-panes is fully supported. See [KeyboardShortcuts]

#### 0.4-beta release (06/10/2013)
Fourth beta release is made today.

- Main feature implemented is split-pane support.
- Use ctrl+shift+h and ctrl+shift+v to create horizontal/vertical split-panes.
- Drag-drop of split-panes is supported as well. To start dragging ctrl + click in terminal or use drag button in terminal top-right menu. Once dragged it can be dropped in other terminals.
- ctrl+shift+Arrow keys to navigate between split-panes. Navigation is still preliminary.

#### 0.3-beta release (05/28/2013)
Third beta release is made today. Many features have been added. Menus (Main menu, Tabbar right-click menu, ctrl-right-click on terminal) are supported. Multiple minor features are in as well. Some features shown in menu (Split session, Import/Export sessions) are still TODO. They will be implemented in next release, which will be soon in couple of weeks.

#### 0.2-beta release (05/17/2013)
Second beta release is made today. I have been using this for a week now, and it is quite stable. One feature of note is automatic loading of saved PuTTY sessions when QuTTY is run first-time. Multiple minor features/fixes were in as well. Visual Studio 2012 Express edition along with Qt 4.8.4 is used for development, and the binary size is down to 2.7MB (Visual Studio Express 2010 with Qt 4.8.1 produced 3.8MB binary).

#### 0.1-beta release (07/08/2012)
First beta release is made today. It works very well as a multi-tabbed SSH/Telnet client. Pretty decent configuration ui similar to PuTTY is supported. Keyboard shortcuts help navigate between tabs. Tmux command mode is integrated as well.

#### Update 05/05/2012
Suriya priya Veluchamy, a new developer has joined us. Stay tuned for Qutty's first release soon.

#### Update 04/10/2012
QuTTY is still in development. For now telnet/ssh connectivity works with some basic features. Still some basic features/configuration options are missing. First release will be made soon once QuTTY supports this. Then we will work on the advanced features similar to iterm2.


[VC++ 2015]: <https://www.microsoft.com/en-us/download/details.aspx?id=48145>
[issue-tracker]: <http://code.google.com/p/qutty/issues/list>
[Features]: <https://code.google.com/p/qutty/wiki/Features>
[TabsInTitlebar]: <https://code.google.com/p/qutty/wiki/TabsInTitlebar>
[HierarchicalSessions]: <https://code.google.com/p/qutty/wiki/HierarchicalSessions>
[KeyboardShortcuts]: <https://code.google.com/p/qutty/wiki/KeyboardShortcuts>
[TmuxIntegration]: <https://code.google.com/p/qutty/wiki/TmuxIntegration>
[HowToBuild]: <https://code.google.com/p/qutty/wiki/HowToBuild>
