#ifndef HELLO_H
#define HELLO_H

#include <QDataStream>
extern const qint64 PROTOCOL_VERSION;
// Two classes to encapsulate the data sent between client and server on initial
// connection.
class ClientHello {
  QString hello;
  qint64 version;
  QString m_username;

 public:
  ClientHello(const QString &username = "");
  bool isValid();
  QString username();
  friend QDataStream &operator<<(QDataStream &out, const ClientHello &m);
  friend QDataStream &operator>>(QDataStream &in, ClientHello &m);
};

// TODO: The server hello's actually includes much more - the actual file &
// history, etc. But it's not included here because that would mean reading the
// file into memory on the server side. (Maybe this will change)
class ServerHello {
  QString hello;
  qint64 version;
  qint64 m_uid;

 public:
  ServerHello(qint64 uid = -1);
  bool isValid();
  qint64 uid();
  friend QDataStream &operator<<(QDataStream &out, const ServerHello &m);
  friend QDataStream &operator>>(QDataStream &in, ServerHello &m);
};

#endif  // HELLO_H
