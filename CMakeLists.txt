#--------------------------------------------------------
# Copyright (C) 2012 Rajendran Thirupugalsamy
# See LICENSE for full copyright and license information.
# See COPYING for distribution information.
#--------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

# Using putty 0.69 source code in puttysrc
#
# settings.c:
# In QuTTY this file is only used for loading saved sessions in
# Windows registry by PuTTY

project(QuTTY VERSION 0.9 LANGUAGES C CXX)

option(HAS_GSSAPI "Enable GSS API" TRUE)

add_compile_definitions(
    IS_QUTTY=1
    QUTTY_RELEASE_VERSION="${PROJECT_VERSION}"
)

if(NOT HAS_GSSAPI)
    add_compile_definitions(NO_GSSAPI=1)
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network Widgets)
find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS Core5Compat)

set(PROJECT_SOURCES
    GuiMain.cpp
    GuiMainWindow.cpp
    GuiTerminalWindow.cpp
    GuiSettingsWindow.cpp
    GuiPreferencesWindow.cpp
    terminalmisc.cpp
    QtCommon.cpp
    QtNet.cpp
    QtUnicode.cpp
    QtPageant.cpp
    QtDlg.cpp
    QtX11.cpp
    QtConfig.cpp
    GuiTabWidget.cpp
    GuiMenu.cpp
    GuiBase.cpp
    GuiSplitter.cpp
    GuiDrag.cpp
    GuiFindToolBar.cpp
    GuiNavigation.cpp
    GuiTreeWidget.cpp
    GuiImportExportFile.cpp
    GuiCompactSettingsWindow.cpp
    GuiTextFilterWindow.cpp
    QtSessionTreeModel.cpp
    QtCompleterWithAdvancedCompletion.cpp

    puttysrc/be_misc.c
    puttysrc/callback.c
    puttysrc/conf.c
    puttysrc/pgssapi.c
    puttysrc/int64.c
    puttysrc/ssh.c
    puttysrc/proxy.c
    puttysrc/portfwd.c
    puttysrc/misc.c
    puttysrc/terminal.c
    puttysrc/ldisc.c
    puttysrc/ldiscucs.c
    puttysrc/miscucs.c
    puttysrc/timing.c
    puttysrc/sshbn.c
    puttysrc/sshpubk.c
    puttysrc/sshrand.c
    puttysrc/sshrsa.c
    puttysrc/sshsh256.c
    puttysrc/sshsh512.c
    puttysrc/sshsha.c
    puttysrc/sshshare.c
    puttysrc/x11fwd.c
    puttysrc/tree234.c
    puttysrc/telnet.c
    puttysrc/pinger.c
    puttysrc/sshmd5.c
    puttysrc/sshaes.c
    puttysrc/sshccp.c
    puttysrc/sshdes.c
    puttysrc/sshecc.c
    puttysrc/sshzlib.c
    puttysrc/sshdh.c
    puttysrc/sshblowf.c
    puttysrc/ssharcf.c
    puttysrc/sshcrcda.c
    puttysrc/sshdss.c
    puttysrc/cproxy.c
    puttysrc/logging.c
    puttysrc/minibidi.c
    puttysrc/sshgssc.c
    puttysrc/wcwidth.c
    puttysrc/sshcrc.c
    puttysrc/wildcard.c
    puttysrc/be_qutty.c
    puttysrc/settings.c
    puttysrc/version.c
    tmux/TmuxBackend.cpp
    tmux/TmuxGateway.cpp
    tmux/TmuxWindowPane.cpp
    tmux/TmuxLayout.cpp
    serialize/QtMRUSessionList.cpp
    serialize/QtWebPluginMap.cpp

    GuiMainWindow.hpp
    GuiTerminalWindow.hpp
    GuiSettingsWindow.hpp
    GuiPreferencesWindow.hpp
    GuiTreeWidget.hpp
    GuiTextFilterWindow.hpp
    GuiImportExportFile.hpp
    QtCommon.hpp
    QtLogDbg.hpp
    QtConfig.hpp
    QtTimer.hpp
    GuiTabWidget.hpp
    GuiMenu.hpp
    GuiBase.hpp
    GuiSplitter.hpp
    GuiDrag.hpp
    GuiFindToolBar.hpp
    GuiTabBar.hpp
    GuiNavigation.hpp
    GuiCompactSettingsWindow.hpp
    QtSessionTreeModel.hpp
    QtSessionTreeItem.hpp
    QtComboBoxWithTreeView.hpp
    QtCompleterWithAdvancedCompletion.hpp

    puttysrc/ssh.h
    puttysrc/putty.h
    puttysrc/QtStuff.h
    puttysrc/puttyps.h
    puttysrc/network.h
    puttysrc/misc.h
    puttysrc/pageant.h
    puttysrc/puttymem.h
    puttysrc/terminal.h
    puttysrc/tree234.h
    puttysrc/sshgss.h
    puttysrc/pgssapi.h
    puttysrc/sshblowf.h
    puttysrc/sshbn.h
    puttysrc/sshgssc.h
    puttysrc/proxy.h
    puttysrc/int64.h
    puttysrc/ldisc.h
    puttysrc/storage.h
    puttysrc/version.h
    tmux/tmux.h
    tmux/TmuxGateway.hpp
    tmux/TmuxWindowPane.hpp
    tmux/TmuxLayout.hpp
    serialize/QtMRUSessionList.hpp
    serialize/QtWebPluginMap.hpp

    GuiSettingsWindow.ui
    GuiPreferencesWindow.ui

    qutty.ico
    QuTTY.qrc
)

set(BREAKPAD_SOURCES
    third-party/google-breakpad/client/windows/handler/exception_handler.cc
    third-party/google-breakpad/client/windows/crash_generation/crash_generation_client.cc
    third-party/google-breakpad/common/windows/guid_string.cc

    third-party/google-breakpad/client/windows/common/ipc_protocol.h
    third-party/google-breakpad/client/windows/crash_generation/crash_generation_client.h
    third-party/google-breakpad/client/windows/handler/exception_handler.h
    third-party/google-breakpad/common/scoped_ptr.h
    third-party/google-breakpad/common/windows/guid_string.h
    third-party/google-breakpad/common/windows/string_utils-inl.h
    third-party/google-breakpad/google_breakpad/common/breakpad_types.h
    third-party/google-breakpad/google_breakpad/common/minidump_cpu_amd64.h
    third-party/google-breakpad/google_breakpad/common/minidump_cpu_arm.h
    third-party/google-breakpad/google_breakpad/common/minidump_cpu_ppc.h
    third-party/google-breakpad/google_breakpad/common/minidump_cpu_ppc64.h
    third-party/google-breakpad/google_breakpad/common/minidump_cpu_sparc.h
    third-party/google-breakpad/google_breakpad/common/minidump_cpu_x86.h
    third-party/google-breakpad/google_breakpad/common/minidump_exception_win32.h
    third-party/google-breakpad/google_breakpad/common/minidump_exception_linux.h
    third-party/google-breakpad/google_breakpad/common/minidump_exception_mac.h
    third-party/google-breakpad/google_breakpad/common/minidump_exception_ps3.h
    third-party/google-breakpad/google_breakpad/common/minidump_exception_solaris.h
    third-party/google-breakpad/google_breakpad/common/minidump_format.h
)

if (WIN32)
    # winnoise.c and winstore.c must be built without UNICODE nor _UNICODE
    set_property(SOURCE
        ${PROJECT_SOURCES}
        puttysrc/windows/windefs.c
        PROPERTY COMPILE_DEFINITIONS UNICODE _UNICODE
    )
    list(APPEND PROJECT_SOURCES
        qutty.rc

        puttysrc/windows/winnoise.c
        puttysrc/windows/winstore.c
        puttysrc/windows/windefs.c
    )
    if(HAS_GSSAPI)
        list(APPEND PROJECT_SOURCES
            puttysrc/windows/wingss.c
        )
    endif()
    set_property(SOURCE
        puttysrc/windows/winstore.c PROPERTY COMPILE_DEFINITIONS
        verify_host_key=verify_host_key_disabled
        store_host_key=store_host_key_disabled
    )
    link_libraries(user32 advapi32)
endif()

if (LINUX)
    list(APPEND PROJECT_SOURCES
        puttysrc/unix/gtkwin.c
        puttysrc/unix/uxnoise.c
        puttysrc/unix/uxstore.c
        puttysrc/unix/unix.h
    )
    if(HAS_GSSAPI)
        list(APPEND PROJECT_SOURCES
            puttysrc/unix/uxgss.c
        )
    endif()
endif()

include_directories(./ puttysrc/)

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/Zc:preprocessor)
    add_link_options($<$<CONFIG:Release>:/MANIFEST:EMBED>)

    # enable google-breakpad support for release mode in MSVC
    include_directories("$<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/third-party/google-breakpad/>")
    foreach(SOURCE IN LISTS BREAKPAD_SOURCES)
        list(APPEND PROJECT_SOURCES "$<$<CONFIG:Release>:${SOURCE}>")
    endforeach()
    add_link_options(
        $<$<CONFIG:Release>:/MAP>
        $<$<CONFIG:Release>:/debug>
        $<$<CONFIG:Release>:/opt:ref>
    )
    add_compile_definitions($<$<CONFIG:Release>:QUTTY_ENABLE_BREAKPAD_SUPPORT=1>)
endif()

if (MINGW)
    add_compile_options(-Wreorder -Wunused -fpermissive -Wdelete-non-virtual-dtor)
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(QuTTY
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
    target_link_libraries(QuTTY PRIVATE Qt${QT_VERSION_MAJOR}::Core5Compat)

    # Define target properties for Android with Qt 6 as:
    #    set_property(TARGET qutty APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
    #                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
    # For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(QuTTY SHARED
            ${PROJECT_SOURCES}
        )
        # Define properties for Android with Qt 5 after find_package() calls as:
        #    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(QuTTY
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(QuTTY PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.qutty)
endif()
set_target_properties(QuTTY PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS QuTTY
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (WIN32)
    if(QT_VERSION_MAJOR GREATER_EQUAL 6)
        qt_disable_unicode_defines(QuTTY)
    endif()
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(QuTTY)
endif()
