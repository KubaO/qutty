#include "QtWebPluginMap.hpp"

#include <QDataStream>
#include <QDir>
#include <QFile>

static const char *serialize_file_name = "qutty/qt_web_plugin_map.txt";

QtWebPluginMap qutty_web_plugin_map;

void QtWebPluginMap::initialize() {
  QFile file(QDir::home().filePath(serialize_file_name));
  if (file.open(QFile::ReadOnly)) {
    QDataStream stream(&file);
    stream >> hash_map;
  }
}

void QtWebPluginMap::save() {
  QFile file(QDir::home().filePath(serialize_file_name));
  if (file.open(QFile::WriteOnly | QFile::Truncate)) {
    QDataStream stream(&file);
    stream << hash_map;
  }
}
