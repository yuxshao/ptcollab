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

  // This is fine to call immediately after calling connecttohost because that
  // fn call opens the socket.
  // TODO: This is not fine. The connection might fail.
  qDebug() << "Sending hello to server";
  m_data_stream << ClientHello(username);
}

void Client::sendAction(const ClientAction &m) {
  // These open guards might cause a desync in case the socket isn't open but
  // the client still thinks something's up. Hopefully this i sunlikely.
  if (m_socket->isOpen()) m_data_stream << m;
}

void Client::sendActionWithData(ClientAction &&m) {
  // No const here b/c the data pointer changes
  if (m_socket->isOpen()) m_data_stream << m;
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
      } catch (const std::string &e) {
        qWarning(
            "Could not read server action. Error: %s. "
            "Discarding",
            e.c_str());
        m_data_stream.rollbackTransaction();
        return;
      }
      if (action.shouldBeRecorded())
        qDebug() << QDateTime::currentDateTime().toString(
                        "yyyy.MM.dd hh:mm:ss.zzz")
                 << "Received" << action;

      if (!(m_data_stream.commitTransaction())) return;

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
