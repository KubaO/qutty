#ifndef QTCONFIG_H
#define QTCONFIG_H

extern "C" {
#include "putty.h"
}
#undef debug  // clashes with debug in qlogging.h

#include <QFile>
#include <QIODevice>
#include <QKeySequence>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringView>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

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

class PuttyConfig {
  class ConfFreeDeleter {
   public:
    void operator()(Conf *ptr) { conf_free(ptr); }
  };

  QString _name;
  std::unique_ptr<Conf, ConfFreeDeleter> _ptr;

  PuttyConfig(Conf *ptr, QStringView name) : _name(name), _ptr(ptr) {}
  PuttyConfig(Conf *ptr, const char *name) : _name(name), _ptr(ptr) {}

 public:
  PuttyConfig() = default;
  PuttyConfig(PuttyConfig &&o) = default;
  PuttyConfig &operator=(PuttyConfig &&o) = default;
  // operator Conf *() const { return _ptr; }
  Conf *get() const { return _ptr.get(); }
  const QString &name() const { return _name; }
  PuttyConfig copy() const { return {conf_copy(_ptr.get()), _name}; }
  PuttyConfig copyWithNewName(QStringView name) const { return {conf_copy(_ptr.get()), name}; }
  void changeName(QStringView name) { _name = QString(name); }
  static PuttyConfig make(const char *name) { return {conf_new(), name}; }
  static PuttyConfig make(QStringView name) { return {conf_new(), name}; }
};

class QtConfig : public QObject {
  Q_OBJECT

 public:
  std::map<QString, PuttyConfig> config_list;
  std::map<std::string, std::string> ssh_host_keys;

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
