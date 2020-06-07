#ifndef SEQUENCINGSERVER_H
#define SEQUENCINGSERVER_H

#include <QFile>
#include <QTcpServer>

#include "ServerSession.h"
#include "protocol/RemoteAction.h"
// TODO : Rename this broadcastserver
class BroadcastServer : public QObject {
  Q_OBJECT
 public:
  BroadcastServer(QString filename, int port, QObject *parent = nullptr);
  ~BroadcastServer();
  int port();

 private slots:
  void newClient();

 private:
  void broadcastRemoteAction(const RemoteActionWithUid &m);
  void broadcastEditState(const EditStateWithUid &m);
  void broadcastNewSession(const QString &username, qint64 uid);
  void broadcastDeleteSession(qint64 uid);
  QTcpServer *m_server;
  QList<RemoteActionWithUid> m_history;
  std::list<ServerSession *> m_sessions;
  QFile m_file;
  int m_next_uid;
};

#endif  // SEQUENCINGSERVER_H
