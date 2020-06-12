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
class Client : public QObject {
  Q_OBJECT
 public:
  Client(QObject *parent);

  HostAndPort currentlyConnectedTo();
  void connectToServer(QString hostname, quint16 port, QString username);
  void sendRemoteAction(const RemoteAction &m);
  void sendEditState(const EditState &m);
  void sendAddUnit(qint32 woice_id, QString woice_name, QString unit_name);
  qint64 uid();
 signals:
  void connected(pxtnDescriptor &desc,
                 const QList<RemoteActionWithUid> &history, qint64 uid);
  void disconnected();
  void receivedRemoteAction(const RemoteActionWithUid &m);
  void receivedEditState(const EditStateWithUid &m);
  void receivedNewSession(const QString &username, qint64 uid);
  void receivedDeleteSession(qint64 uid);
  void receivedAddUnit(qint32 woice_id, QString woice_name, QString unit_name,
                       qint64 uid);
  void errorOccurred(QString error);

 private:
  QTcpSocket *m_socket;
  QDataStream m_data_stream;
  bool m_received_hello;
  qint64 m_uid;
  void tryToRead();
  void tryToStart();
};

#endif  // ACTIONCLIENT_H
