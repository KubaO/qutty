#ifndef QTCONFIG_H
#define QTCONFIG_H

extern "C" {
#include "putty.h"
}
#include <QIODevice>
#include <QKeySequence>
#include <QString>
#include <QPoint>
#include <QSize>
#include <map>
#include <vector>
#include <string>
#include <stddef.h>
#include <QFile>

class QtMenuActionConfig {
 public:
  uint32_t id;
  QKeySequence shortcut;
  QString name;
  QString str_data;
  uint32_t int_data;

  QtMenuActionConfig(uint32_t _id, const QKeySequence &k, QString n = {}, QString s = {},
                     uint32_t i = 0)
      : id(_id), shortcut(k), name(n), str_data(s), int_data(i) {}
};

typedef struct qutty_mainwindow_settings_t__ {
  QSize size;
  QPoint pos;
  int state;
  int flag;
  bool menubar_visible;
  bool titlebar_tabs;
} qutty_mainwindow_settings_t;

class QtConfig : public QObject {
  Q_OBJECT

  class ConfFreeDeleter {
   public:
    void operator()(Conf *ptr) { conf_free(ptr); }
  };

 public:
  using Pointer = std::unique_ptr<Conf, ConfFreeDeleter>;

  static Pointer copy(Conf *cfg) { return Pointer(conf_copy(cfg)); }

  std::map<std::string, std::string> ssh_host_keys;
  std::map<QString, Pointer> config_list;
  std::map<uint32_t, QtMenuActionConfig> menu_action_list;
  qutty_mainwindow_settings_t mainwindow;

  bool restoreConfig();
  bool saveConfig();
  void importFromFile(QFile *);
  void importFromPutty();
  void exportToFile(QFile *);

signals:
  void savedSessionsChanged();

 private:
  int readFromXML(QIODevice *device);
  int writeToXML(QIODevice *device);
  bool restoreFromPuttyWinRegistry();
};

// all global config is here
extern QtConfig qutty_config;

extern std::vector<std::string> qutty_string_split(const std::string &str, char delim);

#define QUTTY_DEFAULT_CONFIG_SETTINGS "Default Settings"
#define QUTTY_SESSION_NAME_SPLIT '/'

bool confKeyExists(Conf *conf, config_primary_key pri, int sec = 0);

#if 0
inline int conf_get_int(QtConfig::Pointer conf, int key) { return conf_get_int(conf.get(), key); }
inline const char *conf_get_str(QtConfig::Pointer conf, int key) {
  return conf_get_str(conf.get(), key);
}
inline const Filename *conf_get_filename(QtConfig::Pointer conf, int key) {
  return conf_get_filename(conf.get(), key);
}
inline const FontSpec *conf_get_fontspec(QtConfig::Pointer conf, int key) {
  return conf_get_fontspec(conf.get(), key);
}
inline int conf_get_int_int(QtConfig::Pointer conf, int key, int subkey) {
  return conf_get_int_int(conf.get(), key, subkey);
}
inline const char *conf_get_str_str(QtConfig::Pointer conf, const char *key, const char *subkey) {
  return conf_get_str_str(conf.get(), key, subkey);
}
#endif

#endif  // QTCONFIG_H
