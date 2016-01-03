/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include <QApplication>
#include <QDir>
#include "GuiMainWindow.h"
#include "GuiTerminalWindow.h"
#include "GuiSettingsWindow.h"

#ifdef QUTTY_ENABLE_BREAKPAD_SUPPORT
#include "client/windows/handler/exception_handler.h"
#endif

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

  // restore all settings from qutty.xml
  qutty_config.restoreConfig();

  GuiMainWindow *mainWindow = new GuiMainWindow();

  mainWindow->on_openNewTab();
  mainWindow->show();

  return app.exec();
}
