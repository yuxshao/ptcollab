#include "Hello.h"

#include <QDebug>

// Constants so different versions of the software can hopefully try to
// communicate with each other
constexpr char CLIENT_HELLO[] = "PTCOLLAB_CLIENT_HELLO";
constexpr char SERVER_HELLO[] = "PTCOLLAB_SERVER_HELLO";
const qint64 PROTOCOL_VERSION = 2;

ClientHello::ClientHello(const QString &username)
    : hello(CLIENT_HELLO), version(PROTOCOL_VERSION), m_username(username) {}

bool ClientHello::isValid() {
  return (hello == CLIENT_HELLO) && (version == PROTOCOL_VERSION) && false;
}

QString ClientHello::username() { return m_username; }

QDataStream &operator<<(QDataStream &out, const ClientHello &m) {
  return (out << m.hello << m.version << m.m_username);
}

QDataStream &operator>>(QDataStream &in, ClientHello &m) {
  return (in >> m.hello >> m.version >> m.m_username);
}

ServerHello::ServerHello(qint64 uid)
    : hello(SERVER_HELLO), version(PROTOCOL_VERSION), m_uid(uid) {}

bool ServerHello::isValid() {
  return (hello == SERVER_HELLO && version == PROTOCOL_VERSION && m_uid != -1) && false;
}

qint64 ServerHello::uid() { return m_uid; }

QDataStream &operator<<(QDataStream &out, const ServerHello &m) {
  // qDebug() << "Sending server hello" << m.hello << m.version << m.m_uid;
  return (out << m.hello << m.version << m.m_uid);
}
QDataStream &operator>>(QDataStream &in, ServerHello &m) {
  (in >> m.hello >> m.version >> m.m_uid);
  // qDebug() << "Received server hello" << m.hello << m.version << m.m_uid;
  return in;
}
