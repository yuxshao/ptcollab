#include "BroadcastServer.h"

#include <QDataStream>
#include <QDateTime>
#include <QMessageBox>
#include <QTcpSocket>

BroadcastServer::BroadcastServer(const QByteArray &data, int port,
                                 QObject *parent)
    : QObject(parent),
      m_server(new QTcpServer(this)),
      m_sessions(),
      m_data(data),
      m_next_uid(0) {
  if (!m_server->listen(QHostAddress::Any, port))
    throw QString("Unable to start TCP server: %1")
        .arg(m_server->errorString());

  qInfo() << "Listening on" << m_server->serverPort();

  connect(m_server, &QTcpServer::newConnection, this,
          &BroadcastServer::newClient);
}

BroadcastServer::~BroadcastServer() {
  // Without these disconnects, I think the sessions' destructors emit the
  // socket disconnected signal which ends up trying to call
  // [broadcastDeleteSession] even after this destructor's been called.
  for (ServerSession *s : m_sessions) s->disconnect();
  m_server->close();
}

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

            session->sendHello(m_data, m_history, sessionMapping(m_sessions));
            connect(session, &ServerSession::receivedAction, this,
                    &BroadcastServer::broadcastAction);
          });
}

void BroadcastServer::broadcastServerAction(const ServerAction &a) {
  if (a.shouldBeRecorded())
    qDebug() << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz")
             << "Broadcast to" << m_sessions.size() << a;

  for (ServerSession *s : m_sessions) s->sendAction(a);
  if (a.shouldBeRecorded()) m_history.push_back(a);
}

void BroadcastServer::broadcastAction(const ClientAction &m, qint64 uid) {
  broadcastServerAction({uid, m});
}

void BroadcastServer::broadcastNewSession(const QString &username, qint64 uid) {
  broadcastServerAction({uid, NewSession{username}});
}

void BroadcastServer::broadcastDeleteSession(qint64 uid) {
  broadcastServerAction({uid, DeleteSession{}});
}
