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

void ServerSession::sendHello(QFile &file, const QList<ServerAction> &history,
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

void ServerSession::sendAction(const ServerAction &a) { m_data_stream << a; }

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
      ClientAction action;
      try {
        m_data_stream.startTransaction();
        m_data_stream >> action;
        if (!(m_data_stream.commitTransaction())) return;
        emit receivedAction(action, m_uid);
      } catch (const std::string &e) {
        qWarning(
            "Could not read client action from %lld (%s). Error: %s. "
            "Discarding",
            m_uid, m_username.toStdString().c_str(), e.c_str());
        m_data_stream.rollbackTransaction();
      }
    }
  }
}
