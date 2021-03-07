#include "LocalServerSession.h"

LocalServerSession::LocalServerSession(QObject *parent, const QString &username,
                                       qint64 uid,
                                       const HostAndPort &connected_to)
    : AbstractServerSession(uid, parent),
      m_username(username),
      m_connected_to(connected_to) {}

void LocalServerSession::sendHello(const QByteArray &file,
                                   const QList<ServerAction> &history,
                                   const QMap<qint64, QString> &sessions) {
  emit clientReceivedHello(file, history, sessions);
}

void LocalServerSession::sendAction(const ServerAction &action) {
  emit clientReceivedAction(action);
}

QString LocalServerSession::username() const { return m_username; }

void LocalServerSession::clientSendHello() { emit receivedHello(); }

void LocalServerSession::clientSendAction(const ClientAction &m) {
  emit receivedAction(m, uid());
}

void LocalServerSession::clientDisconnect() { emit disconnected(); }

const HostAndPort &LocalServerSession::connectedTo() const {
  return m_connected_to;
}
