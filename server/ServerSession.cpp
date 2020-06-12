#include "ServerSession.h"

#include <QDebug>
#include <QHostAddress>

#include "protocol/Hello.h"

constexpr qint64 chunkSize = 8 * 1024 * 4;  // arbitrary 4KB
ServerSession::ServerSession(QObject *parent, QTcpSocket *conn, qint64 uid)
    : QObject(parent),
      m_conn(conn),
      m_data_stream((QIODevice *)conn),
      m_uid(uid),
      m_username(""),
      m_received_hello(false) {
  connect(m_conn, &QIODevice::readyRead, this, &ServerSession::readMessage);
  connect(m_conn, &QAbstractSocket::disconnected, [this]() {
    m_conn->deleteLater();
    // m_state = ServerSession::DISCONNECTED;
    m_conn = nullptr;
    emit disconnected();
  });
}

void ServerSession::sendHello(QFile &file,
                              const QList<RemoteActionWithUid> &history,
                              const QMap<qint64, QString> &sessions) {
  qInfo() << "Sending hello to " << m_conn->peerAddress();
  if (!file.isOpen()) {
    qFatal("Server cannot open file");
    return;
  }
  file.seek(0);

  // send over the file + whole history
  m_data_stream.setVersion(QDataStream::Qt_5_14);
  m_data_stream << ServerHello(m_uid);

  std::unique_ptr<char[]> buffer = std::make_unique<char[]>(chunkSize);
  qint64 bytesLeft = file.size();
  m_data_stream << bytesLeft;
  while (bytesLeft > 0) {
    qint64 bytesRead = file.read(buffer.get(), std::min(chunkSize, bytesLeft));
    if (bytesRead == -1) return;
    bytesLeft -= bytesRead;
    qDebug() << "Sending" << bytesRead;
    m_data_stream.writeRawData(buffer.get(), bytesRead);
  }

  m_data_stream << history << sessions;
}

void ServerSession::sendRemoteAction(const RemoteActionWithUid &m) {
  m_data_stream << FromServer::REMOTE_ACTION << m;
}

void ServerSession::sendEditState(const EditStateWithUid &m) {
  m_data_stream << FromServer::EDIT_STATE << m;
}

void ServerSession::sendNewSession(const QString &username, qint64 uid) {
  m_data_stream << FromServer::NEW_SESSION << username << uid;
}

void ServerSession::sendDeleteSession(qint64 uid) {
  m_data_stream << FromServer::DELETE_SESSION << uid;
}
void ServerSession::sendAddUnit(qint32 woice_id, QString woice_name,
                                QString unit_name, qint64 uid) {
  m_data_stream << FromServer::ADD_UNIT << woice_id << woice_name << unit_name
                << uid;
}

qint64 ServerSession::uid() const { return m_uid; }

QString ServerSession::username() const { return m_username; }

void ServerSession::readMessage() {
  while (!m_data_stream.atEnd()) {
    m_data_stream.startTransaction();
    if (!m_received_hello) {
      ClientHello m;
      m_data_stream >> m;
      if (!m_data_stream.commitTransaction()) return;
      m_received_hello = true;
      m_username = m.username();
      emit receivedHello();
    } else {
      m_data_stream.startTransaction();
      FromClient::MessageType type;
      m_data_stream >> type;
      switch (type) {
        case FromClient::REMOTE_ACTION: {
          RemoteAction m;
          m_data_stream >> m;
          if (!(m_data_stream.commitTransaction())) return;
          emit receivedRemoteAction(RemoteActionWithUid{m, m_uid});
        } break;
        case FromClient::EDIT_STATE: {
          EditState m;
          m_data_stream >> m;
          if (!(m_data_stream.commitTransaction())) return;
          emit receivedEditState(EditStateWithUid{m, m_uid});
        } break;
        case FromClient::ADD_UNIT: {
          qint32 woice_id;
          QString woice_name, unit_name;
          m_data_stream >> woice_id >> woice_name >> unit_name;
          if (!(m_data_stream.commitTransaction())) return;
          emit receivedAddUnit(woice_id, woice_name, unit_name, m_uid);
        } break;
      }
    }
  }
}
