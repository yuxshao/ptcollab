#ifndef SEQUENCINGSERVER_H
#define SEQUENCINGSERVER_H

#include <QFile>
#include <QTcpServer>

#include "ServerSession.h"
#include "protocol/RemoteAction.h"
class SequencingServer : public QObject {
  Q_OBJECT
 public:
  SequencingServer(QString filename, int port, QObject *parent = nullptr);
  ~SequencingServer();
  int port();

 private slots:
  void newClient();

 private:
  void broadcastRemoteAction(const RemoteActionWithUid &m);
  void broadcastEditState(const EditStateWithUid &m);
  QTcpServer *m_server;
  QList<RemoteActionWithUid> m_history;
  std::list<ServerSession *> m_sessions;
  QFile m_file;
  int m_next_uid;
};

#endif  // SEQUENCINGSERVER_H
