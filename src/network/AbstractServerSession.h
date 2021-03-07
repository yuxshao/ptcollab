#ifndef ABSTRACTSERVERSESSION_H
#define ABSTRACTSERVERSESSION_H

#include "protocol/Data.h"
#include "protocol/RemoteAction.h"
class AbstractServerSession : public QObject {
  Q_OBJECT
 public:
  AbstractServerSession(qint64 uid, QObject *parent);
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

#endif  // ABSTRACTSERVERSESSION_H
