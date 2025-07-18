#include "QtConfig.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QObject>
#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "serialize/QtMRUSessionList.hpp"
#include "serialize/QtWebPluginMap.hpp"

extern "C" {
#include "putty.h"
#include "storage.h"
#include "tree234.h"
}

static const int keyFor(const char *keyName) {
  static const std::unordered_map<std::string, config_primary_key> KEYS = {
#define CONF_OPTION(key, type, ...) {#key, CONF_##key},
#include "conf.h"
#undef CONF_OPTION
  };
  auto pk = KEYS.find(keyName);
  if (pk != KEYS.end()) return pk->second;
  return -1;
}

extern "C" const char *conf_id(int key);

static const char *nameFor(config_primary_key key) { return conf_id(key); /* in conf_debug.c */ }

struct KeyOption {
  config_primary_key key;
  int count;
};

static const KeyOption &optionFor(int key) {
  static const KeyOption OPTIONS[] = {
      {CONF_colours, CONF_NCOLOURS}, {CONF_wordness, 256},
      {CONF_ssh_kexlist, KEX_MAX},   {CONF_ssh_cipherlist, CIPHER_MAX},
      {CONF_ssh_hklist, HK_MAX},
#ifndef NO_GSSAPI
      {CONF_ssh_gsslist, 3},
#endif
      {N_CONFIG_OPTIONS, -1}};
  for (const KeyOption &opt : OPTIONS) {
    if (opt.key == key) return opt;
  }
  return OPTIONS[sizeof(OPTIONS) / sizeof(KeyOption) - 1];
}

struct conf_tag {
  tree234 *tree;
};

bool confKeyExists(Conf *conf, config_primary_key pri, int sec) {
  struct {
    int pri;
    union {
      int i;
      const char *s;
    } sec;
  } akey = {pri, sec};
  auto *entry = find234(conf->tree, &akey, NULL);
  if (!entry) qDebug() << "Missing key " << nameFor(pri) << sec;
  return entry != nullptr;
}

struct Read {
  QXmlStreamAttributes attrs;
  QByteArray keyName;
  QByteArray value;
  const KeyOption &opt;
  int _key;

  explicit Read(QXmlStreamReader &xml)
      : attrs(xml.attributes()),
        keyName(attrs.value("key").toLatin1()),
        value(attrs.value("value").toLatin1()),
        opt(optionFor(keyFor(keyName))),
        _key(keyFor(keyName)) {}

  QStringView operator[](QAnyStringView attrName) const { return attrs.value(attrName); }
  bool hasAttribute(QAnyStringView attrName) const { return attrs.hasAttribute(attrName); }
  config_primary_key key() const { return config_primary_key(_key); }
  bool validKey() const { return _key >= 0; }
};

static void read_BOOL(QXmlStreamReader &xml, Conf *conf) {
  auto r = Read(xml);
  bool ok;
  int v = r.value.toInt(&ok);
  if (ok) {
    conf_set_bool(conf, r.key(), v);
  }
  xml.skipCurrentElement();
}

static void read_INT(QXmlStreamReader &xml, Conf *conf, int pri = -1, int sec = -1) {
  auto r = Read(xml);
  bool ok;
  int v = r.value.toInt(&ok);
  if (pri < 0) pri = r.key();
  if (ok && pri >= 0) {
    if (sec < 0)
      conf_set_int(conf, pri, v);
    else
      conf_set_int_int(conf, pri, sec, v);
  }
  xml.skipCurrentElement();
}

static void read_COLOUR(QXmlStreamReader &xml, Conf *conf, int pri, int sec) {
  auto r = Read(xml);
  bool ok;
  int v = r.value.toInt(&ok, 16);
  if (ok) {
    sec *= 3;
    conf_set_int_int(conf, pri, sec + 0, (v >> 16) & 0xFF);
    conf_set_int_int(conf, pri, sec + 1, (v >> 8) & 0xFF);
    conf_set_int_int(conf, pri, sec + 2, (v >> 0) & 0xFF);
  }
  xml.skipCurrentElement();
}

static void read_STR(QXmlStreamReader &xml, Conf *conf) {
  auto r = Read(xml);
  if (r.validKey()) conf_set_str(conf, r.key(), r.value);
  xml.skipCurrentElement();
}

static void read_FILENAME(QXmlStreamReader &xml, Conf *conf) {
  auto r = Read(xml);
  if (r.validKey()) {
    Filename *fn = filename_from_utf8(r.value);
    conf_set_filename(conf, r.key(), fn);
    filename_free(fn);
  }
  xml.skipCurrentElement();
}

static QtMenuActionConfig read_MENUACTION(QXmlStreamReader &xml) {
  auto r = Read(xml);
  uint32_t key = r.keyName.toInt();
  QKeySequence seq(r["keysequence"].toString());
  QtMenuActionConfig action(key, seq);
  action.name = r["name"].toUtf8();
  action.str_data = r["str_data"].toUtf8();
  if (r.hasAttribute("int_data")) action.int_data = r["int_data"].toInt();
  xml.skipCurrentElement();
  return action;
}

static void read_FONT(QXmlStreamReader &xml, Conf *conf) {
  auto r = Read(xml);
  if (r.validKey()) {
    FontSpec fs;
    strncpy(fs.name, r["name"].toLatin1(), sizeof(fs.name));
    fs.name[sizeof(fs.name) - 1] = '\0';
    fs.height = r["height"].toInt();
    fs.isbold = r["bold"].toInt();
    fs.charset = r["charset"].toInt();
    conf_set_fontspec(conf, r.key(), &fs);
  }
  xml.skipCurrentElement();
}

static void read_LIST(QXmlStreamReader &xml, Conf *conf) {
  auto r = Read(xml);
  if (r.validKey()) {
    int i = 0;
    while (xml.readNextStartElement()) {
      if (xml.name() == "int") {
        read_INT(xml, conf, r.key(), i);
        i++;
      } else if (xml.name() == "colour") {
        read_COLOUR(xml, conf, r.key(), i);
        i++;
      } else
        xml.skipCurrentElement();
    }
  } else
    xml.skipCurrentElement();
}

static void read_DICT_STR(QXmlStreamReader &xml, Conf *conf) {
  auto r = Read(xml);
  if (r.validKey()) {
    while (xml.readNextStartElement()) {
      if (xml.name() == "str") {
        auto rr = Read(xml);
        conf_set_str_str(conf, r.key(), rr.keyName, rr.value);
        xml.skipCurrentElement();  // str
      } else
        xml.skipCurrentElement();
    }
  } else
    xml.skipCurrentElement();
}

static std::map<std::string, std::string> read_DICT_STR(QXmlStreamReader &xml) {
  std::map<std::string, std::string> result;
  while (xml.readNextStartElement()) {
    if (xml.name() == "str") {
      auto rr = Read(xml);
      result[rr.keyName.data()] = rr.value;
      xml.skipCurrentElement();  // str
    } else
      xml.skipCurrentElement();
  }
  return result;
}

static std::vector<QtMenuActionConfig> read_DICT_MENUACTION(QXmlStreamReader &xml) {
  std::vector<QtMenuActionConfig> result;
  while (xml.readNextStartElement()) {
    auto attr = xml.attributes();
    if (xml.name() == "MenuAction" && attr.hasAttribute("key") &&
        attr.hasAttribute("keysequence")) {
      result.push_back(read_MENUACTION(xml));
    } else
      xml.skipCurrentElement();
  }
  return result;
}

int QtConfig::readFromXML(QIODevice *device) {
  QXmlStreamReader xml(device);

  if (!xml.readNextStartElement() || xml.name() != "qutty" ||
      xml.attributes().value("version") != "2.0") {
    QMessageBox::warning(NULL, QObject::tr("Qutty Configuration"), QObject::tr("Invalid xml file"));
    return false;
  }
  while (xml.readNextStartElement()) {
    if (xml.name() == "config" && xml.attributes().value("version") == "2.0") {
      QString configName = xml.attributes().value("name").toString();
      PuttyConfig cfg = PuttyConfig::make(configName);
      load_open_settings(nullptr, cfg.get());  // pre-populate with defaults
      while (xml.readNextStartElement()) {
        if (xml.name() == "bool")
          read_BOOL(xml, cfg.get());
        else if (xml.name() == "int")
          read_INT(xml, cfg.get());
        else if (xml.name() == "str")
          read_STR(xml, cfg.get());
        else if (xml.name() == "Filename")
          read_FILENAME(xml, cfg.get());
        else if (xml.name() == "FontSpec")
          read_FONT(xml, cfg.get());
        else if (xml.name() == "list")
          read_LIST(xml, cfg.get());
        else if (xml.name() == "dict")
          read_DICT_STR(xml, cfg.get());
        else
          xml.skipCurrentElement();
      }
      config_list[configName] = std::move(cfg);
    } else if (xml.name() == "sshhostkeys" && xml.attributes().value("version") == "2.0") {
      while (xml.readNextStartElement()) {
        if (xml.name() == "dict")
          ssh_host_keys = read_DICT_STR(xml);
        else
          xml.skipCurrentElement();
      }
    } else if (xml.name() == "keyboardshortcuts" && xml.attributes().value("version") == "2.0") {
      while (xml.readNextStartElement()) {
        if (xml.name() == "dict") {
          auto const actions = read_DICT_MENUACTION(xml);
          for (auto &action : actions) menu_action_list.insert(std::make_pair(action.id, action));
        } else
          xml.skipCurrentElement();
      }
    } else
      xml.skipCurrentElement();
  }
  return 0;
}

static void write(QXmlStreamWriter &xml, const char *type, const char *keyName,
                  QAnyStringView value) {
  xml.writeStartElement(type);
  if (keyName) xml.writeAttribute("key", keyName);
  xml.writeAttribute("value", value);
  xml.writeEndElement();
}

static void write_INT(QXmlStreamWriter &xml, Conf *conf, config_primary_key key,
                      const char *keyName /*nullable*/, int i = -1) {
  if (!confKeyExists(conf, key, std::min(0, i))) return;
  int value = (i < 0) ? conf_get_int(conf, key) : conf_get_int_int(conf, key, i);
  write(xml, "int", keyName, QString::number(value));
}

static void write_BOOL(QXmlStreamWriter &xml, Conf *conf, config_primary_key key,
                       const char *keyName /*nullable*/, int i = -1) {
  if (!confKeyExists(conf, key, 0)) return;
  bool value = conf_get_bool(conf, key);
  write(xml, "bool", keyName, QString::number(value));
}

static void write_COLOUR(QXmlStreamWriter &xml, Conf *conf, config_primary_key pri, int sec) {
  sec *= 3;
  if (!confKeyExists(conf, pri, sec + 0) || !confKeyExists(conf, pri, sec + 1) ||
      !confKeyExists(conf, pri, sec + 2))
    return;
  int r = conf_get_int_int(conf, pri, sec + 0);
  int g = conf_get_int_int(conf, pri, sec + 1);
  int b = conf_get_int_int(conf, pri, sec + 2);
  write(xml, "colour", nullptr, QString::asprintf("%02X%02X%02X", r, g, b));
}

static void write_STR(QXmlStreamWriter &xml, Conf *conf, config_primary_key key,
                      const char *keyName) {
  if (!confKeyExists(conf, key, 0)) return;
  const char *value = conf_get_str(conf, key);
  write(xml, "str", keyName, QString::fromLocal8Bit(value));
}

static void write_STR_AMBI(QXmlStreamWriter &xml, Conf *conf, config_primary_key key,
                           const char *keyName) {
  if (!confKeyExists(conf, key, 0)) return;
  bool utf8;
  const char *value = conf_get_str_ambi(conf, key, &utf8);
  if (utf8)
    write(xml, "str", keyName, value);
  else {
    QString text = QString::fromLocal8Bit(value);
    write(xml, "str", keyName, text);  // will be encoded in utf8 by the writer
  }
}

static void write_UTF8(QXmlStreamWriter &xml, Conf *conf, config_primary_key key,
                       const char *keyName) {
  if (!confKeyExists(conf, key, 0)) return;
  const char *value = conf_get_utf8(conf, key);
  write(xml, "str", keyName, value);  // will be encoded in utf8 by the writer
}

static void write_FILENAME(QXmlStreamWriter &xml, Conf *conf, config_primary_key key,
                           const char *keyName) {
  if (!confKeyExists(conf, key, 0)) return;
  const Filename *fn = conf_get_filename(conf, key);
  const char *utf8 = filename_to_utf8(fn);
  write(xml, "Filename", keyName, utf8);
  sfree((void *)utf8);
}

static void write_FONT(QXmlStreamWriter &xml, Conf *conf, config_primary_key key,
                       const char *keyName) {
  if (!confKeyExists(conf, key, 0)) return;
  const FontSpec *font = conf_get_fontspec(conf, key);
  xml.writeStartElement("FontSpec");
  xml.writeAttribute("key", keyName);
  xml.writeAttribute("name", font->name);
  xml.writeAttribute("height", QString::number(font->height));
  xml.writeAttribute("bold", QString::number(font->isbold));
  xml.writeAttribute("charset", QString::number(font->charset));
  xml.writeEndElement();
}

static void write_INT_sINT(QXmlStreamWriter &xml, Conf *conf, config_primary_key key,
                           const char *keyName) {
  xml.writeStartElement("list");
  xml.writeAttribute("key", keyName);
  const KeyOption &opt = optionFor(key);
  for (int i = 0; i < opt.count; i++) {
    if (key == CONF_colours)
      write_COLOUR(xml, conf, key, i);
    else
      write_INT(xml, conf, key, nullptr, i);
  }
  xml.writeEndElement();
}

static void write_STR_sSTR(QXmlStreamWriter &xml, Conf *conf, config_primary_key key,
                           const char *keyName) {
  xml.writeStartElement("dict");
  xml.writeAttribute("key", keyName);
  char *subkey = nullptr;
  const char *value = nullptr;
  while ((value = conf_get_str_strs(conf, key, subkey, &subkey))) {
    write(xml, "str", subkey, value);
  }
  xml.writeEndElement();
}

static void write_STR_sSTR(QXmlStreamWriter &xml, const std::map<std::string, std::string> &map) {
  xml.writeStartElement("dict");
  for (auto &it : map) {
    write(xml, "str", it.first.c_str(), it.second);
  }
  xml.writeEndElement();
}

static void write_MENUACTION(QXmlStreamWriter &xml, const QtMenuActionConfig &mac) {
  xml.writeStartElement("MenuAction");
  xml.writeAttribute("key", QString::number(mac.id));
  xml.writeAttribute("keysequence", mac.shortcut.toString());
  if (!mac.name.isEmpty()) xml.writeAttribute("name", mac.name);
  if (!mac.str_data.isEmpty()) xml.writeAttribute("str_data", mac.str_data);
  if (mac.int_data) xml.writeAttribute("int_data", QString::number(mac.int_data));
  xml.writeEndElement();
}

int QtConfig::writeToXML(QIODevice *device) {
  QXmlStreamWriter xml;
  xml.setDevice(device);
  xml.setAutoFormatting(true);

  xml.writeStartDocument();
  xml.writeDTD("<!DOCTYPE qutty>");
  xml.writeStartElement("qutty");
  xml.writeAttribute("version", "2.0");

  if (!menu_action_list.empty()) {
    xml.writeStartElement("keyboardshortcuts");
    xml.writeAttribute("version", "2.0");
    xml.writeStartElement("dict");
    for (auto it = menu_action_list.begin(); it != menu_action_list.end(); it++) {
      assert(it->first == it->second.id);
      write_MENUACTION(xml, it->second);
    }
    xml.writeEndElement();
    xml.writeEndElement();
  }

  for (auto it = config_list.cbegin(); it != config_list.cend(); it++) {
    Conf *conf = it->second.get();
    qDebug() << conf;
    xml.writeStartElement("config");
    xml.writeAttribute("version", "2.0");
    xml.writeAttribute("name", it->first);

#define VALUE_TYPE(x) _##x
#define SUBKEY_TYPE(x) _s##x
#define DEFAULT_INT(x)
#define DEFAULT_STR(x)
#define DEFAULT_BOOL(x)
#define SAVE_KEYWORD(x)
#define STORAGE_ENUM(x)
#define LOAD_CUSTOM
#define SAVE_CUSTOM
#define NOT_SAVED
#define CONF_OPTION2(key, type1, type2, ...) write##type2##type1(xml, conf, CONF_##key, #key);
#define CONF_OPTION(...) CONF_OPTION2(__VA_ARGS__)
#include "conf.h"
    xml.writeEndElement();
  }

  xml.writeStartElement("sshhostkeys");
  xml.writeAttribute("version", "2.0");
  write_STR_sSTR(xml, ssh_host_keys);
  xml.writeEndElement();

  xml.writeEndDocument();
  return 0;
}

bool QtConfig::restoreConfig() {
  bool rc = true;
  config_list.clear();
  QFile file(QDir::home().filePath("qutty.xml"));

  if (!file.exists()) {
    restoreFromPuttyWinRegistry();
    if (this->config_list.size() > 0) {
      rc = this->saveConfig();
      if (rc) {
        QMessageBox::information(NULL, QObject::tr("Qutty first-time Configuration"),
                                 QObject::tr("Automatically loaded %1 saved sessions from PuTTY")
                                     .arg(this->config_list.size() - 1));
      } else {
        QMessageBox::warning(NULL, QObject::tr("Qutty first-time Configuration"),
                             QObject::tr("Failed to save %1 saved sessions from PuTTY")
                                 .arg(this->config_list.size() - 1));
      }
    }
  }

  if (!file.exists()) {
    PuttyConfig cfg = PuttyConfig::make(QUTTY_DEFAULT_CONFIG_SETTINGS);
    load_open_settings(nullptr, cfg.get());
    qutty_config.config_list[cfg.name()] = std::move(cfg);
    saveConfig();
  }
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    QMessageBox::warning(
        NULL, QObject::tr("Qutty Configuration"),
        QObject::tr("Cannot read file %1:\n%2.").arg(file.fileName(), file.errorString()));
    return false;
  }
  readFromXML(&file);
  emit savedSessionsChanged();

  // restore any other serialized data strcutures
  qutty_mru_sesslist.initialize();

  qutty_web_plugin_map.initialize();
  return rc;
}

void QtConfig::exportToFile(QFile *file) {
  if (!file->open(QFile::WriteOnly | QFile::Text)) {
    QMessageBox::warning(
        NULL, QObject::tr("Qutty Configuration"),
        QObject::tr("Cannot write file %1:\n%2.").arg(file->fileName(), file->errorString()));
    return;
  }
  writeToXML(file);
}

#ifdef _WIN32

struct enumsettings {
  HKEY key;
  int i;
};

enumsettings *enum_sshhostkey_start(void) {
  struct enumsettings *ret;
  HKEY key;

  if (RegOpenKeyA(HKEY_CURRENT_USER, PUTTY_REG_POS "\\SshHostKeys", &key) != ERROR_SUCCESS)
    return NULL;

  ret = snew(struct enumsettings);
  if (ret) {
    ret->key = key;
    ret->i = 0;
  }

  return ret;
}

int enum_sshhostkey_next(enumsettings *e, char *hostkey, DWORD hostkeylen,
                         unsigned char *hostkey_val, DWORD hostkey_val_len) {
  return RegEnumValueA(e->key, e->i++, hostkey, &hostkeylen, NULL, NULL, hostkey_val,
                       &hostkey_val_len) == ERROR_SUCCESS;
}

void enum_sshhostkey_finish(enumsettings *e) {
  RegCloseKey(e->key);
  sfree(e);
}

#endif

/*
 * Windows only: Try loading the sessions stored in registry by PUTTY
 */
bool QtConfig::restoreFromPuttyWinRegistry() {
#ifdef _WIN32
  bool rc = true;
  struct sesslist savedSess;

  get_sesslist(&savedSess, TRUE);
  qDebug() << "putty nsessions " << savedSess.nsessions;
  for (int i = 0; i < savedSess.nsessions; i++) {
    const char *configName = savedSess.sessions[i];
    PuttyConfig cfg = PuttyConfig::make(configName);

    settings_r *sesskey = open_settings_r(configName);
    load_open_settings(sesskey, cfg.get());
    close_settings_r(sesskey);

    qDebug() << "putty session " << i << " name " << savedSess.sessions[i] << " host "
             << conf_get_str(cfg.get(), CONF_host) << " port "
             << conf_get_int(cfg.get(), CONF_port);
    this->config_list[cfg.name()] = std::move(cfg);
  }

  // load ssh hostkey list from registry
  enumsettings *handle = enum_sshhostkey_start();
  char hostkey[512];
  unsigned char hostkey_val[2048];
  if (handle) {
    while (
        enum_sshhostkey_next(handle, hostkey, sizeof(hostkey), hostkey_val, sizeof(hostkey_val))) {
      this->ssh_host_keys[(char *)hostkey] = std::string_view((char *)hostkey_val);
    }
    enum_sshhostkey_finish(handle);
  }

  get_sesslist(&savedSess, FALSE); /* free */

  return rc;
#else
  return true;
#endif
}

bool QtConfig::saveConfig() {
  QFile file(QDir::home().filePath("qutty.xml"));
  if (!file.open(QFile::WriteOnly | QFile::Text)) {
    QMessageBox::warning(
        NULL, QObject::tr("Qutty Configuration"),
        QObject::tr("Cannot write file %1:\n%2.").arg(file.fileName(), file.errorString()));
    return false;
  }
  writeToXML(&file);
  emit savedSessionsChanged();
  return true;
}

void QtConfig::importFromFile(QFile *file) {
  if (!file->open(QFile::ReadWrite | QFile::Text)) {
    QMessageBox::warning(
        NULL, QObject::tr("Qutty Configuration"),
        QObject::tr("Cannot read file %1:\n%2.").arg(file->fileName(), file->errorString()));
    return;
  }

  readFromXML(file);
}

void QtConfig::importFromPutty() { restoreFromPuttyWinRegistry(); }
