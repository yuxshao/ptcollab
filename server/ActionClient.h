#ifndef ACTIONCLIENT_H
#define ACTIONCLIENT_H
#include <QObject>
#include <QTcpSocket>

#include "protocol/RemoteAction.h"
#include "pxtone/pxtnDescriptor.h"
class ActionClient : public QObject {
  Q_OBJECT
 public:
  ActionClient(QObject *parent, QString hostname, quint16 port);

  void sendRemoteAction(const RemoteAction &action);
  int uid();
 signals:
  void ready(pxtnDescriptor &desc, const QList<RemoteAction> &history);
  void receivedRemoteAction(int uid, const RemoteAction &action);

 private:
  QTcpSocket *m_socket;
  QDataStream m_data_stream;
  bool m_ready;
  int m_uid;
  void tryToRead();
  void tryToStart();
};

#endif  // ACTIONCLIENT_H
