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
  void sendHello(QFile &file, const QList<RemoteActionWithUid> &history,
                 const QMap<qint64, QString> &sessions);
  void sendRemoteAction(const RemoteActionWithUid &action);
  void sendEditState(const EditStateWithUid &m);
  void sendNewSession(const QString &username, qint64 uid);
  void sendDeleteSession(qint64 uid);
  qint64 uid() const;
  QString username() const;
  bool hasReceivedHello() const;

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
  QString m_username;
  // State m_state;
  bool m_received_hello;
};

#endif  // SERVERSESSION_H
