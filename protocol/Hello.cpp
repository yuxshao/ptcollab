#include "Hello.h"

#include <QDebug>

// Constants so different versions of the software can hopefully try to
// communicate with each other
constexpr char CLIENT_HELLO[] = "CLIENT_HELLO";
constexpr qint64 CLIENT_VERSION = 1;
constexpr char SERVER_HELLO[] = "SERVER_HELLO";
constexpr qint64 SERVER_VERSION = 1;

ClientHello::ClientHello(const QString &username)
    : hello(CLIENT_HELLO), version(CLIENT_VERSION), m_username(username) {}

bool ClientHello::isValid() {
  return (hello == CLIENT_HELLO) && (version == CLIENT_VERSION);
}

QString ClientHello::username() { return m_username; }

QDataStream &operator<<(QDataStream &out, const ClientHello &m) {
  return (out << m.hello << m.version << m.m_username);
}

QDataStream &operator>>(QDataStream &in, ClientHello &m) {
  return (in >> m.hello >> m.version >> m.m_username);
}

ServerHello::ServerHello(qint64 uid)
    : hello(SERVER_HELLO), version(SERVER_VERSION), m_uid(uid) {}

bool ServerHello::isValid() {
  return (hello == SERVER_HELLO && version == SERVER_VERSION && m_uid != -1);
}

qint64 ServerHello::uid() { return m_uid; }

QDataStream &operator<<(QDataStream &out, const ServerHello &m) {
  // qDebug() << "sending" << m.hello << m.version << m.m_uid;
  return (out << m.hello << m.version << m.m_uid);
}
QDataStream &operator>>(QDataStream &in, ServerHello &m) {
  // qDebug() << "receiving" << m.hello << m.version << m.m_uid;
  return (in >> m.hello >> m.version >> m.m_uid);
}
