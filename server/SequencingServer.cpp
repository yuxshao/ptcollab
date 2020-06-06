#include "SequencingServer.h"

#include <QDataStream>
#include <QMessageBox>
#include <QTcpSocket>
SequencingServer::SequencingServer(QObject *parent)
    : QObject(parent),
      m_server(new QTcpServer(this)),
      m_sessions(),
      m_file("/tmp/a.ptcop", this),
      m_next_uid(0) {
  if (!m_server->listen()) {
    qFatal("Unable to start TCP server");
    return;
  }
  qInfo() << "Listening on" << m_server->serverPort();

  connect(m_server, &QTcpServer::newConnection, this,
          &SequencingServer::newClient);
}

void SequencingServer::newClient() {
  QTcpSocket *conn = m_server->nextPendingConnection();
  qInfo() << "New connection" << conn->peerAddress();

  m_sessions.emplace_back(this, conn, m_file, m_history);
}

void SequencingServer::broadcastMessage(int uid, const RemoteAction &action) {
  // iterate over list of conns, prune inactive ones, and send action to active
  // ones
  qInfo() << "Broadcasting action" << uid << action.idx;
  int sent = 0;
  for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
    while (!it->isConnected()) it = m_sessions.erase(it);
    if (it == m_sessions.end()) break;
    it->writeRemoteAction(action);
    ++sent;
  }
  qInfo() << "Sent (" << uid << action.idx << ") to" << sent << "clients";
}
