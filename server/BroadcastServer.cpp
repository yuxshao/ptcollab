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
  if (!m_file.open(QIODevice::ReadOnly)) {
    qFatal("File cannot be opened");
    return;
  }
  if (!m_server->listen(QHostAddress::Any, port)) {
    qFatal("Unable to start TCP server");
    return;
  }
  qInfo() << "Listening on" << m_server->serverPort();

  connect(m_server, &QTcpServer::newConnection, this,
          &BroadcastServer::newClient);
}

BroadcastServer::~BroadcastServer() { m_server->close(); }
int BroadcastServer::port() { return m_server->serverPort(); }

void BroadcastServer::newClient() {
  QTcpSocket *conn = m_server->nextPendingConnection();
  qInfo() << "New connection" << conn->peerAddress();

  ServerSession *session = new ServerSession(this, conn, m_next_uid++);
  connect(session, &ServerSession::receivedRemoteAction, this,
          &BroadcastServer::broadcastRemoteAction);
  connect(session, &ServerSession::receivedEditState, this,
          &BroadcastServer::broadcastEditState);
  connect(session, &ServerSession::receivedHello,
          [session, this]() { session->sendHello(m_file, m_history); });
  m_sessions.push_back(session);
}

void BroadcastServer::broadcastRemoteAction(const RemoteActionWithUid &m) {
  // iterate over list of conns, prune inactive ones, and send action to active
  // ones
  qInfo() << "Broadcasting action" << m.uid << m.action.idx;
  int sent = 0;
  for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
    // TODO: could probably make this cleaner. make sure not to accidentally
    // access end
    while (it != m_sessions.end() && !(*it)->isConnected()) {
      qInfo() << "Cleared session" << (*it)->uid();
      delete *it;
      it = m_sessions.erase(it);
    }
    if (it == m_sessions.end()) break;
    (*it)->sendRemoteAction(m);
    ++sent;
  }
  m_history.push_back(m);
  qInfo() << "Sent (" << m.uid << m.action.idx << ") to" << sent << "clients";
}

// TODO: this duplication sucks. but it seems necessary b/c we can't have a
// unified message type like this
void BroadcastServer::broadcastEditState(const EditStateWithUid &m) {
  // Less is done here b/c we're not adding to history
  for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
    while (it != m_sessions.end() && !(*it)->isConnected()) {
      qInfo() << "Cleared session" << (*it)->uid();
      delete *it;
      it = m_sessions.erase(it);
    }
    if (it == m_sessions.end()) break;
    (*it)->sendEditState(m);
  }
}
