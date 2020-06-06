#include "ServerSession.h"

#include <QDebug>
#include <QHostAddress>

constexpr qint64 chunkSize = 8 * 1024 * 4;  // arbitrary 4KB
ServerSession::ServerSession(QObject *parent, QTcpSocket *conn, QFile &file,
                             const QList<RemoteActionWithUid> &history,
                             qint64 uid)
    : QObject(parent),
      m_conn(conn),
      m_data_stream((QIODevice *)conn),
      m_uid(uid) {
  if (!file.isOpen()) {
    qFatal("Server cannot open file");
    return;
  }
  file.seek(0);

  // send over the file + whole history
  m_data_stream.setVersion(QDataStream::Qt_5_14);
  m_data_stream << QString("PXTONE_HISTORY");
  m_data_stream << (qint32)1;  // version

  m_data_stream << m_uid;
  std::unique_ptr<char[]> buffer = std::make_unique<char[]>(chunkSize);
  qint64 bytesLeft = file.size();
  qInfo() << "Sending file to" << conn->peerAddress();
  m_data_stream << bytesLeft;
  qDebug() << "Sent size" << bytesLeft;
  while (bytesLeft > 0) {
    qint64 bytesRead = file.read(buffer.get(), std::min(chunkSize, bytesLeft));
    if (bytesRead == -1) return;
    bytesLeft -= bytesRead;
    qDebug() << "Sending" << bytesRead;
    m_data_stream.writeRawData(buffer.get(), bytesRead);
  }

  qInfo() << "Sending history to" << conn->peerAddress();
  m_data_stream << history;
  qInfo() << "Sent to" << conn->peerAddress();

  connect(conn, &QIODevice::readyRead, this, &ServerSession::readRemoteAction);
  connect(conn, &QAbstractSocket::disconnected, [this]() {
    m_conn->deleteLater();
    m_conn = nullptr;
    emit disconnected();
  });
}

bool ServerSession::isConnected() { return (m_conn != nullptr); }

void ServerSession::writeRemoteAction(const RemoteActionWithUid &action) {
  m_data_stream << action;
}

void ServerSession::readRemoteAction() {
  m_data_stream.startTransaction();
  RemoteAction action;
  m_data_stream >> action;
  if (!(m_data_stream.commitTransaction())) return;
  emit newRemoteAction(RemoteActionWithUid{action, m_uid});
}
