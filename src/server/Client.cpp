#include "Client.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QHostAddress>
#include <QMessageBox>

#include "protocol/Hello.h"
Client::Client(QObject *parent)
    : QObject(parent),
      m_socket(new QTcpSocket(this)),
      m_data_stream((QIODevice *)m_socket),
      m_received_hello(false) {
  connect(m_socket, &QTcpSocket::readyRead, this, &Client::tryToRead);
  connect(m_socket, &QTcpSocket::disconnected, [this]() {
    m_received_hello = false;
    emit disconnected();
  });

  connect(m_socket, &QTcpSocket::errorOccurred,
          [this](QAbstractSocket::SocketError) {
            emit errorOccurred(m_socket->errorString());
          });
}
HostAndPort Client::currentlyConnectedTo() {
  return HostAndPort{m_socket->peerAddress().toString(), m_socket->peerPort()};
}
void Client::connectToServer(QString hostname, quint16 port, QString username) {
  m_socket->abort();
  m_socket->connectToHost(hostname, port);

  // Guarded on connection in case the connection fails. In the past not having
  // this has caused me problems
  QMetaObject::Connection *const conn = new QMetaObject::Connection;
  *conn = connect(m_socket, &QTcpSocket::connected, [this, conn, username]() {
    qDebug() << "Sending hello to server";
    m_data_stream << ClientHello(username);
    disconnect(*conn);
    delete conn;
  });
}

void Client::sendAction(const ClientAction &m) {
  // Sometimes if I try to write data to a socket that's not ready it
  // invalidates the socket forever. I think these two guards should prevent it.
  if (m_socket->isValid() && m_socket->state() == QTcpSocket::ConnectedState) {
    m_data_stream << m;
    if (m_socket->bytesToWrite() == 0) {
      qWarning() << "Client::sendAction didn't seem to fill write buffer.";
      qWarning() << "Socket state: open(" << m_socket->isOpen() << "), valid ("
                 << m_socket->isValid() << "), state(" << m_socket->state()
                 << "), error(" << m_socket->errorString() << ")";
    }
  } else {
    qDebug() << "Not sending action while socket is not ready" << m;
    qDebug() << "Socket state: open(" << m_socket->isOpen() << "), valid ("
             << m_socket->isValid() << "), state(" << m_socket->state()
             << "), error(" << m_socket->errorString() << ")";
  }
}

qint64 Client::uid() { return m_uid; }

void Client::tryToRead() {
  // qDebug() << "Client has bytes available" << m_socket->bytesAvailable();
  // I got tripped up. tryToStart cannot be in the loop b/c that will cause it
  // to loop infinitely. The way it's coded right now at least.
  if (!m_received_hello) tryToStart();
  if (m_received_hello)
    while (!m_data_stream.atEnd()) {
      m_data_stream.startTransaction();

      ServerAction action;
      try {
        m_data_stream >> action;
      } catch (const std::runtime_error &e) {
        qWarning("Discarding unreadable server action. Error: %s. ", e.what());
        m_data_stream.rollbackTransaction();
        return;
      }
      if (!m_data_stream.commitTransaction()) {
        // qDebug() << "Client::tryToRead: Past stream end, can't commit.";
        return;
      }

      if (action.shouldBeRecorded())
        qDebug() << QDateTime::currentDateTime().toString(
                        "yyyy.MM.dd hh:mm:ss.zzz")
                 << "Received" << action;

      emit receivedAction(action);
    }
}

void Client::tryToStart() {
  // Get the file + history
  qInfo() << "Getting initial data from server";

  ServerHello hello;
  QByteArray data;
  QList<ServerAction> history;
  // TODO: actually use sessions using the history state thing from before
  QMap<qint64, QString> sessions;

  m_data_stream.setVersion(QDataStream::Qt_5_5);
  m_data_stream.startTransaction();

  m_data_stream >> hello;
  if (!hello.isValid()) qFatal("Invalid hello response");
  m_uid = hello.uid();

  m_data_stream >> data >> history >> sessions;
  if (!m_data_stream.commitTransaction()) return;

  qDebug() << "Received history of size" << history.size();

  m_received_hello = true;
  pxtnDescriptor d;
  d.set_memory_r(data.constData(), data.size());
  emit connected(d, history, m_uid);
  // for (auto it = sessions.begin(); it != sessions.end(); ++it)
  //  emit receivedNewSession(it.value(), it.key());
}
