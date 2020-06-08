#include "BroadcastServer.h"

#include <QDataStream>
#include <QMessageBox>
#include <QTcpSocket>
BroadcastServer::BroadcastServer(QString filename, int port, QObject *parent)
    : QObject(parent),
      m_server(new QTcpServer(this)),
      m_sessions(),
      m_file(filename, this),
      m_next_uid(0) {
  if (!m_file.open(QIODevice::ReadOnly)) throw QString("File cannot be opened");

  if (!m_server->listen(QHostAddress::Any, port))
    throw QString("Unable to start TCP server: %1")
        .arg(m_server->errorString());

  qInfo() << "Listening on" << m_server->serverPort();

  connect(m_server, &QTcpServer::newConnection, this,
          &BroadcastServer::newClient);
}

BroadcastServer::~BroadcastServer() { m_server->close(); }
int BroadcastServer::port() { return m_server->serverPort(); }
static QMap<qint64, QString> sessionMapping(
    const std::list<ServerSession *> &sessions) {
  QMap<qint64, QString> mapping;
  for (const ServerSession *const s : sessions)
    mapping.insert(s->uid(), s->username());
  return mapping;
}

void BroadcastServer::newClient() {
  QTcpSocket *conn = m_server->nextPendingConnection();
  qInfo() << "New connection" << conn->peerAddress();

  ServerSession *session = new ServerSession(this, conn, m_next_uid++);

  // It's a bit complicated managing responses to the session in response to its
  // state. Key things to be aware of:
  // 1. Don't send remote actions/edit state until received hello.
  // 2. Clean up properly on disconnect.
  auto deleteOnDisconnect = connect(session, &ServerSession::disconnected,
                                    session, &QObject::deleteLater);
  connect(session, &ServerSession::receivedHello,
          [session, deleteOnDisconnect, this]() {
            broadcastNewSession(session->username(), session->uid());
            m_sessions.push_back(session);

            // Track iterator so we can delete it when it goes away
            auto it = --m_sessions.end();
            disconnect(deleteOnDisconnect);
            connect(session, &ServerSession::disconnected,
                    [it, session, this]() {
                      m_sessions.erase(it);
                      broadcastDeleteSession(session->uid());
                      session->deleteLater();
                    });

            session->sendHello(m_file, m_history, sessionMapping(m_sessions));
            connect(session, &ServerSession::receivedRemoteAction, this,
                    &BroadcastServer::broadcastRemoteAction);
            connect(session, &ServerSession::receivedEditState, this,
                    &BroadcastServer::broadcastEditState);
          });
}

// TODO: Try to reduce this duplication even more?
void BroadcastServer::broadcastRemoteAction(const RemoteActionWithUid &m) {
  qInfo() << "Broadcasting action" << m.uid << m.action.idx << "to"
          << m_sessions.size() << "users";
  for (ServerSession *s : m_sessions) s->sendRemoteAction(m);
  m_history.push_back(m);
}

void BroadcastServer::broadcastEditState(const EditStateWithUid &m) {
  for (ServerSession *s : m_sessions) s->sendEditState(m);
}

void BroadcastServer::broadcastNewSession(const QString &username, qint64 uid) {
  for (ServerSession *s : m_sessions) s->sendNewSession(username, uid);
}

void BroadcastServer::broadcastDeleteSession(qint64 uid) {
  for (ServerSession *s : m_sessions) s->sendDeleteSession(uid);
}
