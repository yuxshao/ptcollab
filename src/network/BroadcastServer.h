#ifndef SEQUENCINGSERVER_H
#define SEQUENCINGSERVER_H

#include <QElapsedTimer>
#include <QFile>
#include <QSettings>
#include <QTcpServer>
#include <QTimer>

#include "ServerSession.h"
#include "protocol/Data.h"
#include "protocol/RemoteAction.h"
class BroadcastServer : public QObject {
  Q_OBJECT
 public:
  // TODO: Add a configurable delay
  BroadcastServer(const QByteArray &data, int port, QString load_history = "",
                  QString save_history = "", QObject *parent = nullptr,
                  int delay_msec = 0, double drop_rate = 0);
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
  QByteArray m_data;
  int m_next_uid;
  int m_delay_msec;
  double m_drop_rate;
  std::unique_ptr<QDataStream> m_load_history;
  std::unique_ptr<QDataStream> m_save_history;
  std::unique_ptr<QSettings> m_save_history_metadata;
  QElapsedTimer m_history_elapsed;
  QTimer *m_timer;
  void broadcastServerAction(const ServerAction &a);
  void broadcastUnreliable(const ServerAction &a);
};

#endif  // SEQUENCINGSERVER_H
