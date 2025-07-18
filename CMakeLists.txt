#--------------------------------------------------------
# Copyright (C) 2012 Rajendran Thirupugalsamy
# See LICENSE for full copyright and license information.
# See COPYING for distribution information.
#--------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

# Using putty 0.83 source code in puttysrc
#
# settings.c:
# In QuTTY this file is only used for loading saved sessions in
# Windows registry by PuTTY

project(QuTTY VERSION 0.9 LANGUAGES C CXX)

option(HAS_GSSAPI "Enable GSS API" TRUE)

add_compile_definitions(
    IS_QUTTY=1
    QUTTY_RELEASE_VERSION="${PROJECT_VERSION}"
    HAVE_AES_NI=1
    HAVE_SHA_NI=1
    HAVE_WMEMCHR=1
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
    QtCommon.cpp
    QtNet.cpp
    #QtUnicode.cpp
    QtPageant.cpp
    QtDlg.cpp
    QtX11.c
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

    puttysrc/crypto/aesgcm-clmul.c
    puttysrc/crypto/aesgcm-common.c
    #puttysrc/crypto/aesgcm-neon.c
    puttysrc/crypto/aesgcm-ref-poly.c
    puttysrc/crypto/aesgcm-select.c
    puttysrc/crypto/aesgcm-sw.c
    puttysrc/crypto/aes-common.c
    #puttysrc/crypto/aes-neon.c
    puttysrc/crypto/aes-ni.c
    puttysrc/crypto/aes-select.c
    puttysrc/crypto/aes-sw.c
    puttysrc/crypto/arcfour.c
    puttysrc/crypto/argon2.c
    puttysrc/crypto/blake2.c
    puttysrc/crypto/blowfish.c
    puttysrc/crypto/chacha20-poly1305.c
    puttysrc/crypto/crc32.c
    puttysrc/crypto/des.c
    puttysrc/crypto/diffie-hellman.c
    puttysrc/crypto/dsa.c
    puttysrc/crypto/ecc-arithmetic.c
    puttysrc/crypto/ecc-ssh.c
    puttysrc/crypto/hash_simple.c
    puttysrc/crypto/hmac.c
    puttysrc/crypto/kex-hybrid.c
    puttysrc/crypto/mac.c
    puttysrc/crypto/mac_simple.c
    puttysrc/crypto/md5.c
    puttysrc/crypto/mlkem.c
    puttysrc/crypto/mpint.c
    puttysrc/crypto/ntru.c
    puttysrc/crypto/openssh-certs.c
    puttysrc/crypto/prng.c
    puttysrc/crypto/pubkey-pem.c
    puttysrc/crypto/pubkey-ppk.c
    puttysrc/crypto/pubkey-ssh1.c
    puttysrc/crypto/rfc6979.c
    puttysrc/crypto/rsa.c
    puttysrc/crypto/sha1-common.c
    #puttysrc/crypto/sha1-neon.c
    puttysrc/crypto/sha1-ni.c
    puttysrc/crypto/sha1-select.c
    puttysrc/crypto/sha1-sw.c
    puttysrc/crypto/sha256-common.c
    #puttysrc/crypto/sha256-neon.c
    puttysrc/crypto/sha256-ni.c
    puttysrc/crypto/sha256-select.c
    puttysrc/crypto/sha256-sw.c
    puttysrc/crypto/sha512-common.c
    #puttysrc/crypto/sha512-neon.c
    puttysrc/crypto/sha512-select.c
    puttysrc/crypto/sha512-sw.c
    puttysrc/crypto/sha3.c
    puttysrc/crypto/xdmauth.c

    puttysrc/crypto/aes.h
    puttysrc/crypto/aesgcm.h
    puttysrc/crypto/aesgcm-footer.h
    puttysrc/crypto/blowfish.h
    puttysrc/crypto/ecc.h
    puttysrc/crypto/mlkem.h
    puttysrc/crypto/mpint_i.h
    puttysrc/crypto/ntru.h
    puttysrc/crypto/pubkey-ssh1.c
    puttysrc/crypto/sha1.h
    puttysrc/crypto/sha256.h
    puttysrc/crypto/sha512.h
    puttysrc/crypto/smallmoduli.h

    puttysrc/otherbackends/telnet.c

    puttysrc/proxy/cproxy.c
    puttysrc/proxy/http.c
    puttysrc/proxy/interactor.c
    puttysrc/proxy/proxy.c
    puttysrc/proxy/socks4.c
    puttysrc/proxy/socks5.c
    puttysrc/proxy/sshproxy.c
    puttysrc/proxy/telnet.c

    puttysrc/proxy/proxy.h

    puttysrc/ssh/agentf.c
    puttysrc/ssh/bpp1.c
    puttysrc/ssh/bpp2.c
    puttysrc/ssh/bpp-bare.c
    puttysrc/ssh/censor1.c
    puttysrc/ssh/censor2.c
    puttysrc/ssh/common.c
    puttysrc/ssh/connection1.c
    puttysrc/ssh/connection1-client.c
    puttysrc/ssh/connection2.c
    puttysrc/ssh/connection2-client.c
    puttysrc/ssh/crc-attack-detector.c
    puttysrc/ssh/gssc.c
    puttysrc/ssh/kex2-client.c
    puttysrc/ssh/login1.c
    puttysrc/ssh/mainchan.c
    puttysrc/ssh/nosharing.c
    puttysrc/ssh/pgssapi.c
    puttysrc/ssh/portfwd.c
    puttysrc/ssh/sharing.c
    puttysrc/ssh/ssh.c
    puttysrc/ssh/transient-hostkey-cache.c
    puttysrc/ssh/transport2.c
    puttysrc/ssh/userauth2-client.c
    puttysrc/ssh/verstring.c
    puttysrc/ssh/x11fwd.c
    puttysrc/ssh/zlib.c

    puttysrc/ssh/bpp.h
    puttysrc/ssh/channel.h
    puttysrc/ssh/connection1.h
    puttysrc/ssh/connection2.h
    puttysrc/ssh/gss.h
    puttysrc/ssh/gssc.h
    puttysrc/ssh/pgssapi.h
    puttysrc/ssh/ppl.h
    puttysrc/ssh/server.h
    puttysrc/ssh/signal-list.h
    puttysrc/ssh/transport2.h
    puttysrc/ssh/ttymode-list.h

    puttysrc/stubs/no-print.c
    puttysrc/stubs/null-cipher.c
    puttysrc/stubs/null-key.c
    puttysrc/stubs/null-lp.c
    puttysrc/stubs/null-mac.c
    puttysrc/stubs/null-plug.c
    puttysrc/stubs/null-seat.c
    puttysrc/stubs/null-socket.c

    puttysrc/terminal/bidi.c
    puttysrc/terminal/lineedit.c
    puttysrc/terminal/terminal.c

    puttysrc/terminal/terminal.h

    puttysrc/unicode/ambiguous_wide_chars.h
    puttysrc/unicode/bidi_brackets.h
    puttysrc/unicode/bidi_mirror.h
    puttysrc/unicode/bidi_type.h
    puttysrc/unicode/nonspacing_chars.h
    puttysrc/unicode/wide_chars.h

    puttysrc/utils/antispoof.c
    puttysrc/utils/backend_socket_log.c
    puttysrc/utils/base64_decode.c
    puttysrc/utils/base64_decode_atom.c
    puttysrc/utils/base64_encode.c
    puttysrc/utils/base64_encode_atom.c
    puttysrc/utils/bufchain.c
    #puttysrc/utils/buildinfo.c
    puttysrc/utils/burnstr.c
    puttysrc/utils/burnwcs.c
    puttysrc/utils/cert-expr.c
    #puttysrc/utils/chomp.c
    puttysrc/utils/conf.c
    puttysrc/utils/conf_data.c
    puttysrc/utils/conf_debug.c
    puttysrc/utils/conf_dest.c
    puttysrc/utils/conf_launchable.c
    puttysrc/utils/ctrlparse.c
    #puttysrc/utils/debug.c
    puttysrc/utils/decode_utf8.c
    puttysrc/utils/default_description.c
    puttysrc/utils/dup_mb_to_wc.c
    puttysrc/utils/dup_wc_to_mb.c
    puttysrc/utils/dupcat.c
    puttysrc/utils/dupprintf.c
    puttysrc/utils/dupstr.c
    puttysrc/utils/encode_utf8.c
    #puttysrc/utils/fgetline.c
    puttysrc/utils/host_ca_new_free.c
    puttysrc/utils/host_strchr.c
    puttysrc/utils/host_strchr_internal.c
    puttysrc/utils/host_strcspn.c
    puttysrc/utils/host_strduptrim.c
    puttysrc/utils/host_strrchr.c
    puttysrc/utils/key_components.c
    puttysrc/utils/log_proxy_stderr.c
    puttysrc/utils/logeventf.c
    puttysrc/utils/make_spr_sw_abort_static.c
    puttysrc/utils/marshal.c
    puttysrc/utils/memory.c
    puttysrc/utils/memxor.c
    puttysrc/utils/nullstrcmp.c
    puttysrc/utils/out_of_memory.c
    puttysrc/utils/parse_blocksize.c
    puttysrc/utils/percent_decode.c
    puttysrc/utils/percent_encode.c
    puttysrc/utils/prompts.c
    puttysrc/utils/ptrlen.c
    #puttysrc/utils/read_file_into.c
    puttysrc/utils/seat_connection_fatal.c
    puttysrc/utils/seat_dialog_text.c
    puttysrc/utils/seat_nonfatal.c
    puttysrc/utils/sessprep.c
    puttysrc/utils/sk_free_peer_info.c
    #puttysrc/utils/smemclr.c
    puttysrc/utils/smemeq.c
    puttysrc/utils/spr_get_error_message.c
    puttysrc/utils/ssh2_pick_fingerprint.c
    puttysrc/utils/sshutils.c
    puttysrc/utils/strbuf.c
    puttysrc/utils/string_length_for_printf.c
    puttysrc/utils/stripctrl.c
    puttysrc/utils/tempseat.c
    puttysrc/utils/tree234.c
    puttysrc/utils/validate_manual_hostkey.c
    puttysrc/utils/version.c
    puttysrc/utils/wcwidth.c
    puttysrc/utils/wildcard.c
    puttysrc/utils/write_c_string_literal.c
    puttysrc/utils/x11_dehexify.c
    puttysrc/utils/x11_identify_auth_proto.c
    puttysrc/utils/x11_make_greeting.c
    puttysrc/utils/x11_parse_ip.c
    puttysrc/utils/x11authfile.c
    puttysrc/utils/x11authnames.c

    puttysrc/be_qutty.c
    puttysrc/callback.c
    puttysrc/errsock.c
    puttysrc/ldisc.c
    puttysrc/logging.c
    puttysrc/pinger.c
    puttysrc/settings.c
    puttysrc/sshpubk.c
    puttysrc/sshrand.c
    puttysrc/timing.c

    puttysrc/conf-enums.h
    puttysrc/conf.h
    puttysrc/defs.h
    puttysrc/marshal.h
    puttysrc/misc.h
    puttysrc/mpint.h
    puttysrc/network.h
    puttysrc/pageant.h
    puttysrc/putty.h
    puttysrc/puttymem.h
    puttysrc/specials.h
    puttysrc/ssh.h
    puttysrc/sshcr.h
    puttysrc/storage.h
    puttysrc/tree234.h
    puttysrc/version.h
    puttysrc/x11disp.c

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
    QtSsh.hpp
    QuTTY.hpp

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

# see https://johnfarrier.com/standardizing-the-handling-of-non-source-files-in-cmake-projects-the-config-target/
add_custom_target(config.QuTTY SOURCES
    LICENSE
    README.md
    documentation/Wiki.md
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

if (0)
    # windows/unicode.c must disable functions we implement via Qt
    set(UCS_OVERRIDES
        init_ucs=_init_ucs
        is_dbcs_leadbyte=_is_dbcs_leadbyte
        mb_to_wc=_mb_to_wc
        wc_to_mb=_wc_to_mb
    )
    set_property(SOURCE
        puttysrc/windows/unicode.c
        PROPERTY COMPILE_DEFINITIONS "${UCS_OVERRIDES}"
    )
endif()

if (WIN32)
    # winnoise.c and winstore.c must be built without UNICODE nor _UNICODE
    set_property(SOURCE
        ${PROJECT_SOURCES}
        puttysrc/windows/platform.h
        APPEND PROPERTY COMPILE_DEFINITIONS UNICODE _UNICODE
    )
    list(APPEND PROJECT_SOURCES
        qutty.rc

        puttysrc/windows/noise.c
        puttysrc/windows/storage.c
        puttysrc/windows/unicode.c
        puttysrc/windows/utils/defaults.c
        puttysrc/windows/utils/dputs.c
        puttysrc/windows/utils/registry.c

        puttysrc/windows/platform.h
    )
    if(HAS_GSSAPI)
        list(APPEND PROJECT_SOURCES
            puttysrc/windows/gss.c
        )
    endif()
    set_property(SOURCE
        puttysrc/windows/storage.c PROPERTY COMPILE_DEFINITIONS
        store_host_key=store_host_key_disabled
    )
endif()

if (LINUX)
    list(APPEND PROJECT_SOURCES
        puttysrc/unix/gtkwin.c
        puttysrc/unix/noise.c
        puttysrc/unix/storagee.c
        puttysrc/unix/unix.h
    )
    if(HAS_GSSAPI)
        list(APPEND PROJECT_SOURCES
            puttysrc/unix/gss.c
        )
    endif()
endif()

include_directories(./ puttysrc/ puttysrc/terminal/ puttysrc/windows/)

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
