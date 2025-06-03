/*
 * Copyright (C) 2012 Rajendran Thirupugalsamy
 * See LICENSE for full copyright and license information.
 * See COPYING for distribution information.
 */

#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QPointer>
#include <QTcpSocket>

#include "QtCommon.hpp"
#include "QtSsh.hpp"

extern "C" {
#include "network.h"
#include "putty.h"
}
#undef debug  // clashes with debug in qlogging.h

extern const struct SocketVtable QtSocket_sockvt;

static void sk_tcp_close(Socket *);
static SockAddr *sk_addr_new();

struct SockAddr {
  struct Deleter {
    void operator()(SockAddr *addr) { sk_addr_free(addr); }
  };

  QHostAddress qtaddr;
  QByteArray error;
};

struct QtSocket : Socket {
  struct Deleter {
    void operator()(QtSocket *sock) { sk_close(sock); }
  };

  QTcpSocket qtsock;
  QByteArray error;
  QByteArray outputData;
  std::unique_ptr<SockAddr, SockAddr::Deleter> addr;
  Plug *plug = nullptr;

  int port = -1;

  bool oobinline = false, nodelay = false, keepalive = false, privport = false;

  bool connected = false;
  bool writable = false;
  bool frozen = false;          /* this causes readability notifications to be ignored */
  bool frozen_readable = false; /* this means we missed at least one readability
                                 * notification while we were frozen */
  bool pending_error = false;   /* in case send() returns error */
  bool localhost_only = false;  /* for listening sockets */
};

static void invokePlugClosing(QtSocket *s, PlugCloseType type, QByteArray errStr) {
  QMetaObject::invokeMethod(
      qApp,
      [type](QPointer<QObject> qtsocket, QByteArray error, Plug *plug) {
        if (qtsocket) plug_closing(plug, type, error);
      },
      Qt::QueuedConnection, QPointer<QObject>(&s->qtsock), errStr, s->plug);
}

static void on_connected(QtSocket *s) {
  s->connected = true;
  s->writable = true;
  s->qtsock.setSocketOption(QAbstractSocket::LowDelayOption, s->nodelay);
  s->qtsock.setSocketOption(QAbstractSocket::KeepAliveOption, s->keepalive);

  plug_log(s->plug, s, PLUGLOG_CONNECT_SUCCESS, s->addr.get(), s->port, nullptr, 0);

  // write out any waiting data
  auto sent = s->qtsock.write(s->outputData);
  assert(sent == s->outputData.size());
  s->outputData.clear();
}

static void on_readyRead(QtSocket *s) {
  if (s->frozen) {
    s->frozen_readable = true;
    return;
  }
  char buf[20480];
  do {
    auto len = s->qtsock.read(buf, sizeof(buf));
    noise_ultralight(NOISE_SOURCE_IOLEN, len);
    plug_receive(s->plug, 0, buf, len);
  } while (s->qtsock.bytesAvailable());
}

static void on_bytesWritten(size_t bytes) { noise_ultralight(NOISE_SOURCE_IOLEN, bytes); }

static void on_error(QtSocket *s, QTcpSocket::SocketError err) {
  s->error = s->qtsock.errorString().toLocal8Bit();
  s->writable = false;
  if (!s->connected) {
    /* We have been connecting, but it failed */
    // invokePlugClosing(s, PLUGCLOSE_ERROR, s->error);
    plug_log(s->plug, s, PLUGLOG_CONNECT_FAILED, s->addr.get(), s->port, nullptr, 0);
  }
  s->connected = false;
  s->outputData.clear();
}

static void on_disconnected(QtSocket *s) {
  s->connected = false;
  s->writable = false;
  QByteArray errStr = s->qtsock.errorString().toLocal8Bit();
  if (!errStr.isEmpty()) {
    invokePlugClosing(s, PLUGCLOSE_ERROR, std::move(errStr));
  } else
    invokePlugClosing(s, PLUGCLOSE_NORMAL, {});
}

Socket *sk_new(SockAddr *addr, int port, bool privport, bool oobinline, bool nodelay,
               bool keepalive, Plug *plug) {
  QtSocket *ret = snew(QtSocket);
  new (ret) QtSocket();

  ret->vt = &QtSocket_sockvt;
  ret->plug = plug;
  ret->oobinline = oobinline;
  ret->nodelay = nodelay;
  ret->keepalive = keepalive;
  ret->privport = privport;
  ret->port = port;
  ret->addr.reset(addr);

  if (!addr) {
    ret->error = "Cannot create socket";
    goto cu0;
  }

  QObject::connect(&ret->qtsock, &QTcpSocket::readyRead, qApp, [ret] { on_readyRead(ret); });
  QObject::connect(&ret->qtsock, &QTcpSocket::bytesWritten, qApp, on_bytesWritten);
  QObject::connect(&ret->qtsock, &QTcpSocket::disconnected, qApp, [ret] { on_disconnected(ret); });
  QObject::connect(&ret->qtsock, &QTcpSocket::errorOccurred, qApp,
                   [ret](QTcpSocket::SocketError err) { on_error(ret, err); });
  QObject::connect(&ret->qtsock, &QTcpSocket::connected, qApp, [ret] { on_connected(ret); });

  plug_log(ret->plug, ret, PLUGLOG_CONNECT_TRYING, ret->addr.get(), ret->port, nullptr, 0);
  ret->qtsock.connectToHost(addr->qtaddr, port);

cu0:
  return ret;
}

Socket *sk_newlistener(const char * /*srcaddr*/, int /*port*/, Plug * /*plug*/,
                       bool /*local_host_only*/, int /*orig_address_family*/) {
  // TODO not implemented
  qDebug() << __FUNCTION__ << "NOT IMPL";
  return NULL;
}

QAbstractSocket *sk_getqtsock(Socket *socket) {
  QtSocket *s = static_cast<QtSocket *>(socket);
  return &s->qtsock;
}

static const char *sk_tcp_socket_error(Socket *sock) {
  QtSocket *s = static_cast<QtSocket *>(sock);
  qDebug() << s->error << s->error.isEmpty();
  if (s->error.isEmpty())
    return nullptr;
  else
    return s->error.data();
}

static size_t sk_tcp_write(Socket *sock, const void *data, size_t len) {
  QtSocket *s = static_cast<QtSocket *>(sock);
  if (0) qDebug() << __FUNCTION__ << len;

  if (s->writable) {
    auto sent = s->qtsock.write((const char *)data, len);
  } else {
    s->outputData.append((const char *)data, len);
  }
  return s->qtsock.bytesToWrite();
}

static size_t sk_tcp_write_oob(Socket *sock, const void *data, size_t len) {
  QtSocket *s = static_cast<QtSocket *>(sock);
  assert(s->connected);
  int ret = s->qtsock.write((const char *)data, len);
  qDebug() << "tcp_write_oob ret " << ret << "\n";
  return ret;
}

static void sk_tcp_write_eof(Socket *sock) { qDebug() << __FUNCTION__; }

static void sk_tcp_close(Socket *sock) {
  if (sock) {
    QtSocket *s = static_cast<QtSocket *>(sock);
    s->~QtSocket();
    sfree(s);
  }
}

static void sk_tcp_set_frozen(Socket *sock, bool is_frozen) {
  QtSocket *s = static_cast<QtSocket *>(sock);

  if (s->frozen == is_frozen) return;
  qDebug() << __FUNCTION__ << s << is_frozen;
  s->frozen = is_frozen;
  if (!is_frozen && s->frozen_readable) on_readyRead(s);
  s->frozen_readable = false;
}

static Plug *sk_tcp_plug(Socket *sock, Plug *p) {
  QtSocket *s = static_cast<QtSocket *>(sock);
  Plug *ret = s->plug;
  if (p) s->plug = p;
  return ret;
}

static const struct SocketVtable QtSocket_sockvt = {
    sk_tcp_plug,      sk_tcp_close,      sk_tcp_write,        sk_tcp_write_oob,
    sk_tcp_write_eof, sk_tcp_set_frozen, sk_tcp_socket_error, nullsock_endpoint_info};

bool sk_addr_needs_port(SockAddr *addr) { return true; }

int sk_addrtype(SockAddr *addr) {
  const QHostAddress &a = addr->qtaddr;
  switch (a.protocol()) {
    case QAbstractSocket::IPv4Protocol:
      return ADDRTYPE_IPV4;
    case QAbstractSocket::IPv6Protocol:
      return ADDRTYPE_IPV6;
    default:
      return ADDRTYPE_NAME;
  }
}

void sk_addrcopy(SockAddr *addr, char *buf) {
  const QByteArray str = addr->qtaddr.toString().toUtf8();
  memcpy(buf, str.data(), str.length() + 1);
}

static SockAddr *sk_addr_new() {
  SockAddr *ret = snew(SockAddr);
  new (ret) SockAddr();
  return ret;
}

SockAddr *sk_addr_dup(SockAddr *addr) {
  if (!addr) return nullptr;
  SockAddr *ret = sk_addr_new();
  ret->qtaddr = addr->qtaddr;
  ret->error = addr->error;
  return ret;
}

void sk_addr_free(SockAddr *addr) {
  if (addr) {
    addr->~SockAddr();
    sfree(addr);
  }
}

SockAddr *sk_namelookup(const char *host, char **canonicalname, int address_family) {
  /*
   * DNS lookups only have a synchronous API in PuTTY:
   *
   *     https://www.chiark.greenend.org.uk/~sgtatham/putty/wishlist/async-dns.html
   *     https://web.archive.org/web/20070726170416/http://dillgroup.ucsf.edu/~justin/putty/adns/
   *
   * A workaround would be to wrap the backend in a QObject, and move it to a thread worker pool
   * for initialization - since that's when a name lookup happens. After the initialization has
   * returned, the backend can be moved back to the main thread.
   */
  SockAddr *ret = new SockAddr;

  QHostInfo info = QHostInfo::fromName(host);
  if (info.error() == QHostInfo::NoError) {
    foreach (const QHostAddress &address, info.addresses()) {
      int this_addrtype = address.protocol() == QAbstractSocket::IPv4Protocol
                              ? ADDRTYPE_IPV4
                              : address.protocol() == QAbstractSocket::IPv6Protocol
                                    ? ADDRTYPE_IPV6
                                    : ADDRTYPE_UNSPEC;
      if (this_addrtype == address_family) {
        QString str = info.hostName();
        QByteArray bstr = str.toUtf8();
        const char *cstr = bstr.constData();
        size_t size = 1 + strlen(cstr);
        *canonicalname = snewn(size, char);
        memcpy(*canonicalname, cstr, size);
        ret->qtaddr = address;
        return ret;
      }
    }
  }
  if (info.addresses().size() > 0) {
    QHostAddress a = QHostAddress(info.addresses().at(0));
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
  ret->error = info.errorString().toLocal8Bit();
  return ret;
}

SockAddr *sk_nonamelookup(const char * /*host*/) {
  qDebug() << __FUNCTION__ << "NOT IMPL";
  // TODO not supported for now
  SockAddr *ret = sk_addr_new();
  ret->error = "Not supported";
  return ret;
}

const char *sk_addr_error(SockAddr *addr) {
  if (addr && !addr->error.isEmpty())
    return addr->error;
  else
    return nullptr;
}

bool sk_hostname_is_local(const char *name) {
  return !strcmp(name, "localhost") || !strcmp(name, "::1") || !strncmp(name, "127.", 4);
}

void sk_getaddr(SockAddr *addr, char *buf, int buflen) {
  QByteArray const str = addr->qtaddr.toString().toUtf8();
  if (buflen > str.length())
    buflen = str.length();
  else
    buflen--;
  memcpy(buf, str.data(), buflen);
  buf[buflen] = '\0';
}

bool sk_address_is_local(SockAddr *addr) {
  const QHostAddress &a = addr->qtaddr;
  if (a == QHostAddress::LocalHost || a == QHostAddress::LocalHostIPv6) return true;
  foreach (const QHostAddress &locaddr, QNetworkInterface::allAddresses()) {
    if (a == locaddr) return true;
  }
  return false;
}

bool sk_address_is_special_local(SockAddr *addr) {
  return false; /* no Unix-domain socket analogue here */
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

SockAddr *platform_get_x11_unix_address(const char * /*path*/, int /*displaynum*/) {
  SockAddr *ret = new SockAddr;
  ret->error = "unix sockets not supported on this platform";
  return ret;
}

/* Proxy indirection layer.
 *
 * Calling new_connection transfers ownership of 'addr': the proxy
 * layer is now responsible for freeing it, and the caller shouldn't
 * assume it exists any more.
 *
 * If calling this from a backend with a Seat, you can also give it a
 * pointer to the backend's Interactor trait. In that situation, it
 * might replace the backend's seat with a temporary seat of its own,
 * and give the real Seat to an Interactor somewhere in the proxy
 * system so that it can ask for passwords (and, in the case of SSH
 * proxying, other prompts like host key checks). If that happens,
 * then the resulting 'temp seat' is the backend's property, and it
 * will have to remember to free it when cleaning up, or after
 * flushing it back into the real seat when the network connection
 * attempt completes.
 *
 * You can free your TempSeat and resume using the real Seat when one
 * of two things happens: either your Plug's closing() method is
 * called (indicating failure to connect), or its log() method is
 * called with PLUGLOG_CONNECT_SUCCESS. In the latter case, you'll
 * probably want to flush the TempSeat's contents into the real Seat,
 * of course.
 */
Socket *platform_new_connection(SockAddr * /*addr*/, const char * /*hostname*/, int /*port*/,
                                bool /*privport*/, bool /*oobinline*/, bool /*nodelay*/,
                                bool /*keepalive*/, Plug * /*plug*/, Conf * /*cfg*/,
                                Interactor * /*itr*/) {
  qDebug() << __FUNCTION__ << "NOTIMPL";  // TODO
  return nullptr;
}

/*
 * Simple wrapper on getservbyname(), needed by portfwd.c. Returns the
 * port number, in host byte order (suitable for printf and so on).
 * Returns 0 on failure. Any platform not supporting getservbyname
 * can just return 0 - this function is not required to handle
 * numeric port specifications.
 */
int net_service_lookup(const char *service) { return 0; }
