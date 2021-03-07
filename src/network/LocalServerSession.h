#ifndef LOCALSERVERSESSION_H
#define LOCALSERVERSESSION_H
#include <QDataStream>
#include <QFile>
#include <QPointer>
#include <QTcpSocket>

#include "network/ServerSession.h"
#include "protocol/Data.h"
#include "protocol/RemoteAction.h"

struct HostAndPort {
  QString host;
  int port;
  QString toString();
};

class LocalServerSession : public AbstractServerSession {
  Q_OBJECT
 public:
  LocalServerSession(QObject *parent, const QString &username, qint64 uid);
  void sendHello(const QByteArray &file, const QList<ServerAction> &history,
                 const QMap<qint64, QString> &sessions);
  void sendAction(const ServerAction &action);
  QString username() const;

 signals:
  void helloSent(const QByteArray &file, const QList<ServerAction> &history,
                 const QMap<qint64, QString> &sessions);
  void actionSent(const ServerAction &action);

 private:
  QString m_username;
  bool m_received_hello;
  HostAndPort m_connected_to;
};

class LocalClientSession : public QObject {
  Q_OBJECT
 public:
  LocalClientSession(QObject *parent);
  bool isConnected();
  void connectToServer(LocalServerSession *server_session,
                       const HostAndPort &connected_to);
  void sendHello();
  void sendAction(const ClientAction &action);
  qint64 uid() const;

  void disconnect();
  const HostAndPort &connectedTo() const;
 signals:
  void receivedHello(qint64 uid, const QByteArray &file,
                     const QList<ServerAction> &history,
                     const QMap<qint64, QString> &sessions);
  void receivedAction(const ServerAction &action);
  void disconnected();

 private:
  qint64 m_uid;
  LocalServerSession *m_server_session;
  std::list<QMetaObject::Connection> m_server_qt_connections;
  HostAndPort m_connected_to;
};

#endif  // LOCALSERVERSESSION_H
