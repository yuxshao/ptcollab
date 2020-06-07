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

  connect(conn, &QIODevice::readyRead, this, &ServerSession::readMessage);
  connect(conn, &QAbstractSocket::disconnected, [this]() {
    m_conn->deleteLater();
    m_conn = nullptr;
    emit disconnected();
  });
}

bool ServerSession::isConnected() { return (m_conn != nullptr); }

void ServerSession::sendRemoteAction(const RemoteActionWithUid &m) {
  m_data_stream << REMOTE_ACTION << m;
}

void ServerSession::sendEditState(const EditStateWithUid &m) {
  m_data_stream << EDIT_STATE << m;
}

qint64 ServerSession::uid() { return m_uid; }

void ServerSession::readMessage() {
  m_data_stream.startTransaction();
  MessageType type;
  m_data_stream >> type;
  switch (type) {
    case REMOTE_ACTION: {
      RemoteAction m;
      m_data_stream >> m;
      if (!(m_data_stream.commitTransaction())) return;
      emit receivedRemoteAction(RemoteActionWithUid{m, m_uid});
    } break;
    case EDIT_STATE: {
      EditState m(0, 0);
      m_data_stream >> m;
      if (!(m_data_stream.commitTransaction())) return;
      emit receivedEditState(EditStateWithUid{m, m_uid});
    } break;
  }
}
