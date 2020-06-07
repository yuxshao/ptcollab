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
  m_data_stream << ClientHello(username);
}

void Client::sendRemoteAction(const RemoteAction &m) {
  m_data_stream << FromClient::REMOTE_ACTION << m;
}

void Client::sendEditState(const EditState &m) {
  m_data_stream << FromClient::EDIT_STATE << m;
}

qint64 Client::uid() { return m_uid; }

void Client::tryToRead() {
  while (!m_data_stream.atEnd()) {
    if (!m_received_hello)
      tryToStart();
    else {
      m_data_stream.startTransaction();
      FromServer::MessageType type;
      m_data_stream >> type;
      switch (type) {
        case FromServer::REMOTE_ACTION: {
          RemoteActionWithUid m;
          m_data_stream >> m;
          if (!(m_data_stream.commitTransaction())) return;
          emit receivedRemoteAction(m);
        } break;
        case FromServer::EDIT_STATE: {
          EditStateWithUid m;
          m_data_stream >> m;
          if (!(m_data_stream.commitTransaction())) return;
          emit receivedEditState(m);
        } break;
        case FromServer::NEW_SESSION: {
          QString username;
          qint64 uid;
          m_data_stream >> username >> uid;
          emit receivedNewSession(username, uid);
        } break;
        case FromServer::DELETE_SESSION: {
          qint64 uid;
          m_data_stream >> uid;
          emit receivedDeleteSession(uid);
        } break;
      }
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

  // TODO: just see if you can get rid of this chunked thing. qdatastream might
  // just do it for you. Then maybe the hello can just include the history and
  // data.
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
  QList<RemoteActionWithUid> history;
  QMap<qint64, QString> sessions;
  m_data_stream >> history >> sessions;
  if (!m_data_stream.commitTransaction()) return;

  qDebug() << "Received history of size" << history.size();

  pxtnDescriptor desc;
  desc.set_memory_r(data.get(), size);
  m_received_hello = true;
  emit connected(desc, history, m_uid);
  for (auto it = sessions.begin(); it != sessions.end(); ++it)
    emit receivedNewSession(it.value(), it.key());
}
