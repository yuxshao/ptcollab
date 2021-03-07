#ifndef SERVERSESSION_H
#define SERVERSESSION_H

#include <QDataStream>
#include <QFile>
#include <QTcpSocket>

#include "AbstractServerSession.h"
#include "protocol/Data.h"
#include "protocol/RemoteAction.h"
class ServerSession : public AbstractServerSession {
  Q_OBJECT
 public:
  ServerSession(QObject *parent, QTcpSocket *conn, qint64 uid);
  void sendHello(const QByteArray &file, const QList<ServerAction> &history,
                 const QMap<qint64, QString> &sessions);
  void sendAction(const ServerAction &action);
  QString username() const;

 private slots:
  void readMessage();

 private:
  QTcpSocket *m_socket;
  QDataStream m_write_stream, m_read_stream;
  QString m_username;
  // State m_state;
  bool m_received_hello;
};

#endif  // SERVERSESSION_H
