/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include "QtCommon.hpp"
extern "C" {
#include "network.h"
#include "putty.h"
#include "ssh.h"
}
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkInterface>

struct SockAddr {
  QHostAddress *qtaddr;
  const char *error;
};

static void on_connected(QtSocket *s) {
  QTcpSocket *qtsock = s->qtsock;
  s->connected = true;
  if (s->nodelay) qtsock->setSocketOption(QAbstractSocket::LowDelayOption, true);
}

static void on_readyRead(QtSocket *s) {
  QTcpSocket *qtsock = s->qtsock;

  if (s->frozen) {
    s->frozen_readable = true;
    return;
  }

  do {
    char buf[20480];
    int len = qtsock->read(buf, sizeof(buf));
    noise_ultralight(NOISE_SOURCE_IOLEN, len);

    plug_receive(s->plug, 0, buf, len);
  } while (qtsock->bytesAvailable());
}

static void on_error(QtSocket *s, QTcpSocket::SocketError err) {
  QByteArray errStr = s->qtsock->errorString().toLocal8Bit();
  plug_closing(s->plug, errStr, s->qtsock->error(), 0);
}

static void on_disconnected(QtSocket *s) {
  QByteArray errStr = s->qtsock->errorString().toLocal8Bit();
  plug_closing(s->plug, errStr, s->qtsock->error(), 0);
}

static void sk_tcp_flush(Socket * /*s*/) {}

static const char *sk_tcp_socket_error(Socket *sock) {
  QtSocket *s = container_of(sock, QtSocket, sock);
  return s->error;
}

static size_t sk_tcp_write(Socket *sock, const void *data, size_t len) {
  QtSocket *s = container_of(sock, QtSocket, sock);
  // This is a hack, since connections are made asynchronously.
  // I assume that some time post-0.71, the connection code in PuTTY becomes asynchornous,
  // and this hack won't be necessary anymore. If nothing changes up to and including PuTTY-0.83,
  // then I'll have to submit a patch.
  // TODO
  while (!s->connected && !s->error && !s->pending_error) QCoreApplication::processEvents();
  assert(s->connected);
  int i, j;
  char pr[10000];
  for (i = 0, j = 0; i < len; i++)
    j += sprintf(pr + j, "%u ", (unsigned char)((const char *)data)[i]);
  // qDebug()<<"sk_tcp_write"<<len<<pr;
  int ret = s->qtsock->write((const char *)data, len);
  noise_ultralight(NOISE_SOURCE_IOLEN, len);
  if (ret <= 0) qDebug() << "tcp_write ret " << ret;
  return ret;
}

static size_t sk_tcp_write_oob(Socket *sock, const void *data, size_t len) {
  QtSocket *s = container_of(sock, QtSocket, sock);
  assert(s->connected);
  int ret = s->qtsock->write((const char *)data, len);
  qDebug() << "tcp_write_oob ret " << ret << "\n";
  return ret;
}

static void sk_tcp_close(Socket *sock) {
  QtSocket *s = container_of(sock, QtSocket, sock);

  s->parent = nullptr;
  if (s->child) sk_tcp_close(&s->child->sock);

  bufchain_clear(&s->output_data);

  s->qtsock->disconnect();
  s->qtsock->deleteLater();
  s->qtsock = nullptr;

  sk_addr_free(s->addr);
  sfree(s);
}

static void sk_tcp_set_frozen(Socket *sock, bool is_frozen) {
  QtSocket *s = container_of(sock, QtSocket, sock);

  if (s->frozen == is_frozen) return;
  qDebug() << __FUNCTION__ << s << is_frozen;
  s->frozen = is_frozen;
  if (!is_frozen && s->frozen_readable) on_readyRead(s);
  s->frozen_readable = false;
}

static Plug *sk_tcp_plug(Socket *sock, Plug *p) {
  QtSocket *s = container_of(sock, QtSocket, sock);
  Plug *ret = s->plug;
  if (p) s->plug = p;
  return ret;
}

static const struct SocketVtable QtSocket_sockvt = {sk_tcp_plug,
                                                    sk_tcp_close,
                                                    sk_tcp_write,
                                                    sk_tcp_write_oob,
                                                    nullptr /* TODO write_eof */,
                                                    sk_tcp_flush,
                                                    sk_tcp_set_frozen,
                                                    sk_tcp_socket_error,
                                                    nullptr /* TODO peer_info */};

bool sk_addr_needs_port(SockAddr *addr) { return true; }

int sk_addrtype(SockAddr *addr) {
  const QHostAddress *a = addr->qtaddr;
  switch (a->protocol()) {
    case QAbstractSocket::IPv4Protocol:
      return ADDRTYPE_IPV4;
    case QAbstractSocket::IPv6Protocol:
      return ADDRTYPE_IPV6;
    default:
      return ADDRTYPE_NAME;
  }
}

void sk_addrcopy(SockAddr *addr, char *buf) {
  QHostAddress *a = addr->qtaddr;
  QString str = a->toString();
  QByteArray bstr = str.toUtf8();
  const char *cstr = bstr.constData();
  memcpy(buf, cstr, bstr.length());
}

SockAddr *sk_addr_dup(SockAddr *addr) {
  if (!addr) return NULL;
  SockAddr *ret = new SockAddr;
  ret->qtaddr = new QHostAddress(*addr->qtaddr);
  ret->error = addr->error;
  return ret;
}

void sk_addr_free(SockAddr *addr) {
  if (!addr) return;
  if (addr->qtaddr) delete addr->qtaddr;
  addr->qtaddr = NULL;
  addr->error = NULL;
}

SockAddr *sk_namelookup(const char *host, char **canonicalname, int address_family) {
  SockAddr *ret = new SockAddr;
  ret->error = NULL;
  QHostInfo info = QHostInfo::fromName(host);
  if (info.error() == QHostInfo::NoError) {
    foreach (const QHostAddress &address, info.addresses()) {
      int this_addrtype = address.protocol() == QAbstractSocket::IPv4Protocol
                              ? ADDRTYPE_IPV4
                              : address.protocol() == QAbstractSocket::IPv6Protocol
                                    ? ADDRTYPE_IPV6
                                    : ADDRTYPE_UNSPEC;
      if (this_addrtype == address_family) {
        QHostAddress *a = new QHostAddress(address);
        QString str = info.hostName();
        QByteArray bstr = str.toUtf8();
        const char *cstr = bstr.constData();
        size_t size = 1 + strlen(cstr);
        *canonicalname = snewn(size, char);
        memcpy(*canonicalname, cstr, size);
        ret->qtaddr = a;
        return ret;
      }
    }
  }
  if (info.addresses().size() > 0) {
    QHostAddress *a = new QHostAddress(info.addresses().at(0));
    QString str = info.hostName();
    QByteArray bstr = str.toUtf8();
    const char *cstr = bstr.constData();
    size_t size = 1 + strlen(cstr);
    *canonicalname = snewn(size, char);
    memcpy(*canonicalname, cstr, size);
    ret->qtaddr = a;
    return ret;
  }
  *canonicalname = snewn(1, char);
  *canonicalname[0] = '\0';
  ret->qtaddr = new QHostAddress();
  ret->error =
      info.error() == QHostInfo::HostNotFound
          ? "Host not found"
          : info.error() == QHostInfo::UnknownError ? "Unknown error" : "No IP address found";
  return ret;
}

SockAddr *sk_nonamelookup(const char * /*host*/) {
  // TODO not supported for now
  SockAddr *ret = new SockAddr;
  ret->qtaddr = new QHostAddress();
  ret->error = "Not supported";
  return ret;
}

const char *sk_addr_error(SockAddr *addr) {
  if (!addr) return NULL;
  return addr->error;
}

bool sk_hostname_is_local(const char *name) {
  return !strcmp(name, "localhost") || !strcmp(name, "::1") || !strncmp(name, "127.", 4);
}

void sk_getaddr(SockAddr *addr, char *buf, int buflen) {
  QHostAddress *a = addr->qtaddr;
  QString str = a->toString();
  QByteArray bstr = str.toUtf8();
  const char *cstr = bstr.constData();
  if (buflen > bstr.length()) buflen = bstr.length();
  strncpy(buf, cstr, buflen);
  buf[buflen - 1] = '\0';
}

bool sk_address_is_local(SockAddr *addr) {
  const QHostAddress *a = addr->qtaddr;
  if (*a == QHostAddress::LocalHost || *a == QHostAddress::LocalHostIPv6) return true;
  foreach (const QHostAddress &locaddr, QNetworkInterface::allAddresses()) {
    if (*a == locaddr) return true;
  }
  return false;
}

bool sk_address_is_special_local(SockAddr *addr) {
  return false; /* no Unix-domain socket analogue here */
}

Socket *sk_new(SockAddr *addr, int port, bool privport, bool oobinline, bool nodelay,
               bool keepalive, Plug *plug) {
  QTcpSocket *qtsock = nullptr;
  QtSocket *ret = snew(QtSocket);
  memset(ret, 0, sizeof(QtSocket));

  ret->sock.vt = &QtSocket_sockvt;
  ret->plug = plug;
  bufchain_init(&ret->output_data);
  ret->oobinline = oobinline;
  ret->nodelay = nodelay;
  ret->keepalive = keepalive;
  ret->privport = privport;
  ret->port = port;
  ret->addr = addr;

  if (!addr || !addr->qtaddr) {
    ret->error = "Cannot create socket";
    goto cu0;
  }

  qtsock = new QTcpSocket();
  ret->qtsock = qtsock;

  QObject::connect(qtsock, &QTcpSocket::readyRead, qApp, [ret] { on_readyRead(ret); });
  QObject::connect(qtsock, &QTcpSocket::disconnected, qApp, [ret] { on_disconnected(ret); });
  QObject::connect(qtsock, &QTcpSocket::errorOccurred, qApp,
                   [ret](QTcpSocket::SocketError err) { on_error(ret, err); });
  QObject::connect(qtsock, &QTcpSocket::connected, qApp, [ret] { on_connected(ret); });

  qtsock->connectToHost(*addr->qtaddr, port);

cu0:
  return &ret->sock;
}

Socket *sk_newlistener(const char * /*srcaddr*/, int /*port*/, Plug * /*plug*/,
                       bool /*local_host_only*/, int /*orig_address_family*/) {
  // TODO not implemented
  return NULL;
}

char *get_hostname(void) {
  static char hostname[512];
  QString str = QHostInfo::localHostName();
  QByteArray bstr = str.toUtf8();
  const char *cstr = bstr.constData();
  memcpy(hostname, cstr, bstr.length());
  return hostname;
}

int net_service_lookup(char * /*service*/) {
  // TODO not implemented
  return 0;
}

Socket *sk_register(void * /*sock*/, Plug * /*plug*/) {
  // TODO not implemented
  return NULL;
}

SockAddr *platform_get_x11_unix_address(const char * /*path*/, int /*displaynum*/) {
  /*
  SockAddr *ret = snew(struct SockAddr_tag);
  memset(ret, 0, sizeof(struct SockAddr_tag));
  ret->error = "unix sockets not supported on this platform";
  ret->refcount = 1;
  return ret;
  */
  return NULL;
}

Socket *platform_new_connection(SockAddr * /*addr*/, const char * /*hostname*/, int /*port*/,
                                bool /*privport*/, bool /*oobinline*/, bool /*nodelay*/,
                                bool /*keepalive*/, Plug * /*plug*/, Conf * /*cfg*/) {
  // TODO not yet implemented
  return NULL;
}

int platform_ssh_share(const char *name, Conf *conf, Plug *downplug, Plug *upplug, Socket **sock,
                       char **logtext, char **ds_err, char **us_err, int can_upstream,
                       int can_downstream) {
  return SHARE_NONE;
}
