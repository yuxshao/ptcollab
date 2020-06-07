#ifndef ACTIONCLIENT_H
#define ACTIONCLIENT_H
#include <QObject>
#include <QTcpSocket>

#include "protocol/RemoteAction.h"
#include "pxtone/pxtnDescriptor.h"
struct HostAndPort {
  QString host;
  int port;
};
// TODO: just rename this client
class ActionClient : public QObject {
  Q_OBJECT
 public:
  ActionClient(QObject *parent);

  HostAndPort currentlyConnectedTo();
  void connectToServer(QString hostname, quint16 port);
  void sendRemoteAction(const RemoteAction &m);
  void sendEditState(const EditState &m);
  qint64 uid();
 signals:
  void connected(pxtnDescriptor &desc,
                 const QList<RemoteActionWithUid> &history, qint64 uid);
  void disconnected();
  void receivedRemoteAction(const RemoteActionWithUid &m);
  void receivedEditState(const EditStateWithUid &m);
  void errorOccurred(QString error);

 private:
  QTcpSocket *m_socket;
  QDataStream m_data_stream;
  bool m_ready;
  qint64 m_uid;
  void tryToRead();
  void tryToStart();
};

#endif  // ACTIONCLIENT_H
