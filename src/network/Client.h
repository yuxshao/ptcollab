#ifndef ACTIONCLIENT_H
#define ACTIONCLIENT_H
#include <QObject>
#include <QTcpSocket>

#include "BroadcastServer.h"
#include "protocol/RemoteAction.h"
#include "pxtone/pxtnDescriptor.h"

class Client : public QObject {
  Q_OBJECT
 public:
  Client(QObject *parent);

  HostAndPort currentlyConnectedTo();
  void connectToServer(QString hostname, quint16 port, QString username);
  void connectToLocalServer(BroadcastServer *server, QString username);
  void disconnectFromServerSuppressSignal();
  void sendAction(const ClientAction &m);
  qint64 uid();
 signals:
  void connected(const QByteArray &desc, const QList<ServerAction> &history,
                 qint64 uid);
  void disconnected(bool suppress);
  void receivedAction(const ServerAction &m);
  void errorOccurred(QString error);

 private:
  QTcpSocket *m_socket;
  LocalServerSession *m_local;
  // We have separate streams because QTBUG-63113 prevents writing if we're
  // currently in the middle of a fragmented read.
  QDataStream m_write_stream, m_read_stream;
  bool m_received_hello;
  bool m_suppress_disconnect;
  qint64 m_uid;
  void tryToRead();
  void tryToStart();
  void handleDisconnect();
};

#endif  // ACTIONCLIENT_H
