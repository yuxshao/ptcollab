#ifndef LOCALSERVERSESSION_H
#define LOCALSERVERSESSION_H
#include <QDataStream>
#include <QFile>
#include <QTcpSocket>

#include "network/ServerSession.h"
#include "protocol/Data.h"
#include "protocol/RemoteAction.h"

struct HostAndPort {
  QString host;
  int port;
  QString toString();
};

// Acts as a bit of a makeshift double-ended pipe, though the client can
// technically send data to itself.
class LocalServerSession : public AbstractServerSession {
  Q_OBJECT
 public:
  LocalServerSession(QObject *parent, const QString &username, qint64 uid,
                     const HostAndPort &connected_to);
  void sendHello(const QByteArray &file, const QList<ServerAction> &history,
                 const QMap<qint64, QString> &sessions);
  void sendAction(const ServerAction &action);
  QString username() const;

  void clientSendHello();
  void clientSendAction(const ClientAction &m);
  void clientDisconnect();
  const HostAndPort &connectedTo() const;
 signals:
  void clientReceivedHello(const QByteArray &file,
                           const QList<ServerAction> &history,
                           const QMap<qint64, QString> &sessions);
  void clientReceivedAction(const ServerAction &action);

 private:
  QString m_username;
  bool m_received_hello;
  HostAndPort m_connected_to;
};
#endif  // LOCALSERVERSESSION_H
