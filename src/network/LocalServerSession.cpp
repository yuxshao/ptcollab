#include "LocalServerSession.h"

LocalServerSession::LocalServerSession(QObject *parent, const QString &username,
                                       qint64 uid)
    : AbstractServerSession(uid, parent), m_username(username) {}

void LocalServerSession::sendHello(const QByteArray &file,
                                   const QList<ServerAction> &history,
                                   const QMap<qint64, QString> &sessions) {
  emit helloSent(file, history, sessions);
}

void LocalServerSession::sendAction(const ServerAction &action) {
  emit actionSent(action);
}

QString LocalServerSession::username() const { return m_username; }

LocalClientSession::LocalClientSession(QObject *parent)
    : QObject(parent), m_server_session(nullptr), m_connected_to({"", 0}) {}

bool LocalClientSession::isConnected() { return (m_server_session != nullptr); }
void LocalClientSession::connectToServer(LocalServerSession *s,
                                         const HostAndPort &connected_to) {
  m_server_session = s;
  m_server_qt_connections =
      std::list({connect(m_server_session, &LocalServerSession::actionSent,
                         this, &LocalClientSession::receivedAction),
                 connect(m_server_session, &LocalServerSession::helloSent,
                         [this](const QByteArray &file,
                                const QList<ServerAction> &history,
                                const QMap<qint64, QString> &sessions) {
                           emit receivedHello(m_server_session->uid(), file,
                                              history, sessions);
                         }),
                 connect(m_server_session, &QObject::destroyed, this,
                         &LocalClientSession::disconnect)});
  m_connected_to = connected_to;
}

void LocalClientSession::disconnect() {
  if (m_server_session != nullptr) {
    LocalServerSession *session = m_server_session;
    for (const QMetaObject::Connection &c : m_server_qt_connections)
      QObject::disconnect(c);
    m_server_qt_connections.clear();
    m_server_session = nullptr;
    m_connected_to = {"", 0};
    emit disconnected();

    emit session->disconnected();
  }
}

void LocalClientSession::sendHello() {
  if (m_server_session != nullptr) emit m_server_session->receivedHello();
}

void LocalClientSession::sendAction(const ClientAction &m) {
  if (m_server_session != nullptr)
    emit m_server_session->receivedAction(m, m_server_session->uid());
}

const HostAndPort &LocalClientSession::connectedTo() const {
  return m_connected_to;
}

qint64 LocalClientSession::uid() const { return m_uid; }
