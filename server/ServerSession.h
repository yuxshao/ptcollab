#ifndef SERVERSESSION_H
#define SERVERSESSION_H

#include <QDataStream>
#include <QFile>
#include <QTcpSocket>

#include "protocol/Data.h"
#include "protocol/RemoteAction.h"
class ServerSession : public QObject {
  Q_OBJECT
 public:
  ServerSession(QObject *parent, QTcpSocket *conn, qint64 uid);
  // Probably don't need super complex state right now. Just need to check if
  // hello is here. enum State { STARTING, READY, DISCONNECTED }; State state();
  void sendHello(Data &file, const QList<ServerAction> &history,
                 const QMap<qint64, QString> &sessions);
  void sendAction(const ServerAction &action);
  qint64 uid() const;
  QString username() const;
  bool hasReceivedHello() const;
 signals:
  void receivedAction(const ClientAction &action, qint64 uid);
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
