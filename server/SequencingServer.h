#ifndef SEQUENCINGSERVER_H
#define SEQUENCINGSERVER_H

#include <QFile>
#include <QTcpServer>

#include "ServerSession.h"
#include "protocol/RemoteAction.h"
class SequencingServer : public QObject {
  Q_OBJECT
 public:
  SequencingServer(QString filename, QObject *parent = nullptr);
  int port();

 private slots:
  void newClient();

 private:
  void broadcastMessage(int uid, const RemoteAction &action);
  QTcpServer *m_server;
  QList<RemoteAction> m_history;
  std::list<ServerSession> m_sessions;
  QFile m_file;
  int m_next_uid;
};

#endif  // SEQUENCINGSERVER_H
