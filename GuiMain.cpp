/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include <QApplication>
#include <QDir>
#include "GuiMainWindow.hpp"
#include "GuiTerminalWindow.hpp"
#include "GuiSettingsWindow.hpp"

#ifdef QUTTY_ENABLE_BREAKPAD_SUPPORT
#include "client/windows/handler/exception_handler.h"
#endif

void callback_notify(void *frontend) {
  QTimer::singleShot(0, qApp, [] { while (run_toplevel_callbacks()); });
}

extern "C" toplevel_callback_notify_fn_t notify_frontend;

// Connection sharing is not implemented yet
extern "C" const int share_can_be_downstream;
extern "C" const int share_can_be_upstream;

const int share_can_be_downstream = FALSE;
const int share_can_be_upstream = FALSE;

int main(int argc, char *argv[]) {
  QDir dumps_dir(QDir::home().filePath("qutty/dumps"));
  if (!dumps_dir.exists()) {
    dumps_dir.mkpath(".");
  }

#ifdef QUTTY_ENABLE_BREAKPAD_SUPPORT
  new google_breakpad::ExceptionHandler(dumps_dir.path().toStdWString(), NULL, NULL, NULL,
                                        google_breakpad::ExceptionHandler::HANDLER_ALL);
#endif

  QApplication app(argc, argv);
  notify_frontend = callback_notify;

  // restore all settings from qutty.xml
  qutty_config.restoreConfig();

  GuiMainWindow *mainWindow = new GuiMainWindow();

  mainWindow->on_openNewTab();
  mainWindow->show();

  int rc = app.exec();
  notify_frontend = nullptr;
  return rc;
}
