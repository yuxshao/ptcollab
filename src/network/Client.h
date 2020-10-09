#ifndef ACTIONCLIENT_H
#define ACTIONCLIENT_H
#include <QObject>
#include <QTcpSocket>

#include "protocol/RemoteAction.h"
#include "pxtone/pxtnDescriptor.h"
struct HostAndPort {
  QString host;
  int port;
  QString toString();
};
class Client : public QObject {
  Q_OBJECT
 public:
  Client(QObject *parent);

  HostAndPort currentlyConnectedTo();
  void connectToServer(QString hostname, quint16 port, QString username);
  void sendAction(const ClientAction &m);
  qint64 uid();
 signals:
  void connected(pxtnDescriptor &desc, QList<ServerAction> &history,
                 qint64 uid);
  void disconnected();
  void receivedAction(ServerAction &m);
  void errorOccurred(QString error);

 private:
  QTcpSocket *m_socket;
  // We have separate streams because QTBUG-63113 prevents writing if we're
  // currently in the middle of a fragmented read.
  QDataStream m_write_stream, m_read_stream;
  bool m_received_hello;
  qint64 m_uid;
  void tryToRead();
  void tryToStart();
};

#endif  // ACTIONCLIENT_H
