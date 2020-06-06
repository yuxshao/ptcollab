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
 signals:
  void ready(const pxtnDescriptor &desc, const QList<RemoteAction> &history);
  void receivedRemoteAction(const RemoteAction &action);

 private:
  QTcpSocket *m_socket;
  QDataStream m_data_stream;
  bool m_ready;
  void tryToRead();
  void tryToStart();
};

#endif  // ACTIONCLIENT_H
