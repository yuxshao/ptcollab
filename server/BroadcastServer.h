#ifndef SEQUENCINGSERVER_H
#define SEQUENCINGSERVER_H

#include <QFile>
#include <QTcpServer>

#include "ServerSession.h"
#include "protocol/Data.h"
#include "protocol/RemoteAction.h"
class BroadcastServer : public QObject {
  Q_OBJECT
 public:
  BroadcastServer(QString filename, int port, QObject *parent = nullptr);
  ~BroadcastServer();
  int port();

 private slots:
  void newClient();

 private:
  void broadcastAction(const ClientAction &m, qint64 uid);
  void broadcastNewSession(const QString &username, qint64 uid);
  void broadcastDeleteSession(qint64 uid);
  QTcpServer *m_server;
  QList<ServerAction> m_history;
  std::list<ServerSession *> m_sessions;
  Data m_file;
  int m_next_uid;
  void broadcastServerAction(const ServerAction &a);
};

#endif  // SEQUENCINGSERVER_H
