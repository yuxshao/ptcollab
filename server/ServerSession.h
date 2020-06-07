#ifndef SERVERSESSION_H
#define SERVERSESSION_H

#include <QDataStream>
#include <QFile>
#include <QTcpSocket>

#include "protocol/RemoteAction.h"
class ServerSession : public QObject {
  Q_OBJECT
 public:
  ServerSession(QObject *parent, QTcpSocket *conn, QFile &file,
                const QList<RemoteActionWithUid> &history, qint64 uid);
  bool isConnected();
  void sendRemoteAction(const RemoteActionWithUid &action);
  void sendEditState(const EditStateWithUid &m);
  qint64 uid();

 signals:
  void receivedRemoteAction(const RemoteActionWithUid &action);
  void receivedEditState(const EditStateWithUid &action);
  void disconnected();

 private slots:
  void readMessage();

 private:
  QTcpSocket *m_conn;
  QDataStream m_data_stream;
  qint64 m_uid;
};

#endif  // SERVERSESSION_H
