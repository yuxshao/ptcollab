#ifndef SERVERSESSION_H
#define SERVERSESSION_H

#include <QDataStream>
#include <QFile>
#include <QTcpSocket>

#include "protocol/RemoteAction.h"
class ServerSession : public QObject {
  Q_OBJECT
 public:
  ServerSession(QObject *parent, QTcpSocket *conn, qint64 uid);
  // Probably don't need super complex state right now. Just need to check if
  // hello is here. enum State { STARTING, READY, DISCONNECTED }; State state();
  void sendHello(QFile &file, const QList<RemoteActionWithUid> &history);
  void sendRemoteAction(const RemoteActionWithUid &action);
  void sendEditState(const EditStateWithUid &m);
  qint64 uid();
  bool isConnected();

 signals:
  void receivedRemoteAction(const RemoteActionWithUid &action);
  void receivedEditState(const EditStateWithUid &action);
  void receivedHello();
  void disconnected();

 private slots:
  void readMessage();

 private:
  QTcpSocket *m_conn;
  QDataStream m_data_stream;
  qint64 m_uid;
  // State m_state;
  bool m_received_hello;
};

#endif  // SERVERSESSION_H
