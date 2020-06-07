#ifndef HELLO_H
#define HELLO_H

#include <QDataStream>
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
