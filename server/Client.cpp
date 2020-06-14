#include "Client.h"

#include <QAbstractSocket>
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
  void (QTcpSocket::*errorSignal)(QAbstractSocket::SocketError) =
      &QTcpSocket::error;
  connect(m_socket, errorSignal, [this](QAbstractSocket::SocketError) {
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
  qDebug() << "Sending hello to server";
  m_data_stream << ClientHello(username);
}

void Client::sendAction(const ClientAction &m) {
  // These open guards might cause a desync in case the socket isn't open but
  // the client still thinks something's up. Hopefully this i sunlikely.
  if (m_socket->isOpen()) m_data_stream << m;
}

qint64 Client::uid() { return m_uid; }

void Client::tryToRead() {
  // I got tripped up. tryToStart cannot be in the loop b/c that will cause it
  // to loop infinitely. The way it's coded right now at least.
  if (!m_received_hello) tryToStart();
  if (m_received_hello)
    while (!m_data_stream.atEnd()) {
      m_data_stream.startTransaction();
      try {
        m_data_stream.startTransaction();
        ServerAction action;
        m_data_stream >> action;
        if (!(m_data_stream.commitTransaction())) return;
        emit receivedAction(action);
      } catch (const std::string &e) {
        qWarning(
            "Could not read server action. Error: %s. "
            "Discarding",
            e.c_str());
        m_data_stream.rollbackTransaction();
      }
    }
}

void Client::tryToStart() {
  // Get the file + history
  qInfo() << "Getting initial data from server";

  m_data_stream.setVersion(QDataStream::Qt_5_14);
  m_data_stream.startTransaction();
  ServerHello hello;
  m_data_stream >> hello;
  if (!hello.isValid()) qFatal("Invalid hello response");

  m_uid = hello.uid();

  // TODO: Pretty sure this chunked read is not necessary since QTcpSocket
  // (QAbstractSocket) does its own buffering.
  qint64 size;
  m_data_stream >> size;
  qDebug() << "Expecting file of size" << size;
  std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
  int pos = 0;
  while (pos < size) {
    int read = m_data_stream.readRawData(data.get() + pos, size - pos);
    if (read != -1) pos += read;
    if (read == -1 || m_data_stream.atEnd()) {
      // TODO: might not be right for chunked transfer
      qInfo() << "Not enough data has been sent yet." << pos;
      m_data_stream.rollbackTransaction();
      return;
    }
  }
  qDebug() << "Received file";
  QList<ServerAction> history;
  // TODO: actually use sessions using the history state thing from before
  QMap<qint64, QString> sessions;
  m_data_stream >> history >> sessions;
  if (!m_data_stream.commitTransaction()) return;

  qDebug() << "Received history of size" << history.size();

  pxtnDescriptor desc;
  desc.set_memory_r(data.get(), size);
  m_received_hello = true;
  emit connected(desc, history, m_uid);
  // for (auto it = sessions.begin(); it != sessions.end(); ++it)
  //  emit receivedNewSession(it.value(), it.key());
}