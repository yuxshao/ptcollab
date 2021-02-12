#ifndef SERVERSESSION_H
#define SERVERSESSION_H

#include "protocol/Data.h"
#include "protocol/RemoteAction.h"
class ServerSession : public QObject {
  Q_OBJECT
 public:
  ServerSession(qint64 uid, QObject *parent);
  virtual void sendHello(const QByteArray &file,
                         const QList<ServerAction> &history,
                         const QMap<qint64, QString> &sessions) = 0;
  virtual void sendAction(const ServerAction &action) = 0;
  virtual QString username() const = 0;
  qint64 uid() const;
 signals:
  void receivedAction(const ClientAction &action, qint64 uid);
  void receivedHello();
  void disconnected();

 private:
  qint64 m_uid;
};

#endif  // SERVERSESSION_H
