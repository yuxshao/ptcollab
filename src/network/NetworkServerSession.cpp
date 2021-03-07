#include "NetworkServerSession.h"

#include <QDebug>
#include <QHostAddress>

#include "protocol/Hello.h"

NetworkServerSession::NetworkServerSession(QObject *parent, QTcpSocket *conn,
                                           qint64 uid)
    : ServerSession(uid, parent),
      m_socket(conn),
      m_write_stream((QIODevice *)conn),
      m_read_stream((QIODevice *)conn),
      m_username(""),
      m_received_hello(false) {
  connect(m_socket, &QIODevice::readyRead, this,
          &NetworkServerSession::readMessage);
  connect(m_socket, &QAbstractSocket::disconnected, [this]() {
    qDebug() << "Disconnected" << ServerSession::uid();
    m_socket->deleteLater();
    // m_state = ServerSession::DISCONNECTED;
    m_socket = nullptr;
    emit disconnected();
  });
  m_write_stream.setVersion(QDataStream::Qt_5_5);
  m_read_stream.setVersion(QDataStream::Qt_5_5);
}

// TODO: Include history, sessions, data in hello as a 'server history state'
void NetworkServerSession::sendHello(const QByteArray &data,
                                     const QList<ServerAction> &history,
                                     const QMap<qint64, QString> &sessions) {
  qInfo() << "Sending hello to " << m_socket->peerAddress();

  m_write_stream << ServerHello(uid()) << data << history << sessions;
}

void NetworkServerSession::sendAction(const ServerAction &a) {
  // TODO: Do we also need a isValid and connected guard here?

  if (!m_socket->isValid() || m_socket->state() != QTcpSocket::ConnectedState) {
    qWarning() << "Trying to broadcast to a socket that's not ready?" << a;
    qWarning() << "Socket state: open(" << m_socket->isOpen() << "), valid ("
               << m_socket->isValid() << "), state(" << m_socket->state()
               << "), error(" << m_socket->errorString() << ")";
  }

  // Testing writing to the socket directly
  /* QByteArray arr;
  QDataStream d(&arr, QIODevice::WriteOnly);
  d << a;
  qint64 written = m_socket->write(arr);
  if (written < arr.length()) {
    qWarning() << "ServerSession::sendAction for u" << m_uid
               << "didn't write as much as expected.";
    qWarning() << "Socket state: open(" << m_socket->isOpen() << "), valid ("
               << m_socket->isValid() << "), state(" << m_socket->state()
               << "), error(" << m_socket->errorString() << ")";
  }*/

  m_write_stream << a;
  qint32 beforeFlush = m_socket->bytesToWrite();
  if (beforeFlush == 0) {
    qWarning() << "ServerSession::sendAction for u" << uid()
               << "didn't seem to fill write buffer.";
    qWarning() << "Socket state: open(" << m_socket->isOpen() << "), valid ("
               << m_socket->isValid() << "), state(" << m_socket->state()
               << "), error(" << m_socket->errorString() << ")"
               << "), stream_status(" << m_write_stream.status() << ")";
  }
}

QString NetworkServerSession::username() const { return m_username; }

void NetworkServerSession::readMessage() {
  while (!m_read_stream.atEnd()) {
    if (!m_received_hello) {
      m_read_stream.startTransaction();
      ClientHello m;
      m_read_stream >> m;
      if (!m_read_stream.commitTransaction()) return;
      m_received_hello = true;
      m_username = m.username();
      emit receivedHello();
    } else {
      m_read_stream.startTransaction();
      ClientAction action;
      try {
        m_read_stream >> action;
      } catch (const std::runtime_error &e) {
        qWarning(
            "Could not read client action from %lld (%s). Error: %s. "
            "Discarding",
            uid(), m_username.toStdString().c_str(), e.what());
        m_read_stream.rollbackTransaction();
        return;
      }
      if (!(m_read_stream.commitTransaction())) return;

      /*QString s;
      QTextStream ts(&s);
      ts << action;
      qDebug() << "Read from" << m_uid << "action" << s;*/

      emit receivedAction(action, uid());
    }
  }
}
