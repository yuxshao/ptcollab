#include "Client.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QMessageBox>
Client::Client(QObject *parent)
    : QObject(parent),
      m_socket(new QTcpSocket(this)),
      m_data_stream((QIODevice *)m_socket),
      m_ready(false) {
  connect(m_socket, &QTcpSocket::readyRead, this, &Client::tryToRead);
  connect(m_socket, &QTcpSocket::disconnected, [this]() {
    m_ready = false;
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
void Client::connectToServer(QString hostname, quint16 port) {
  m_socket->abort();
  m_socket->connectToHost(hostname, port);
}

void Client::sendRemoteAction(const RemoteAction &m) {
  m_data_stream << REMOTE_ACTION << m;
}

void Client::sendEditState(const EditState &m) {
  m_data_stream << EDIT_STATE << m;
}

qint64 Client::uid() { return m_uid; }

void Client::tryToRead() {
  if (!m_ready) tryToStart();
  // This second check is not an else b/c tryToStart(); might change m_ready;
  if (m_ready) {
    while (!m_data_stream.atEnd()) {
      m_data_stream.startTransaction();
      MessageType type;
      m_data_stream >> type;
      switch (type) {
        case REMOTE_ACTION: {
          RemoteActionWithUid m;
          m_data_stream >> m;
          if (!(m_data_stream.commitTransaction())) return;
          emit receivedRemoteAction(m);
        } break;
        case EDIT_STATE: {
          EditStateWithUid m{EditState(0, 0), 0};
          m_data_stream >> m;
          if (!(m_data_stream.commitTransaction())) return;
          emit receivedEditState(m);
        } break;
      }
    }
  }
}

void Client::tryToStart() {
  // Get the file + history
  qInfo() << "Getting initial data from server";
  QString header;
  qint32 version;
  m_data_stream.startTransaction();
  m_data_stream >> header >> version;
  qDebug() << "Received header and version" << header << version;
  if (header != "PXTONE_HISTORY") qFatal("Unexpected header");
  if (version != 1) qFatal("Unexpected version");
  m_data_stream.setVersion(QDataStream::Qt_5_14);

  m_data_stream >> m_uid;
  qDebug() << "Received UID" << m_uid;

  qint64 size;
  m_data_stream >> size;
  qDebug() << "Expecting file of size" << size;
  char *data = new char[size];
  int pos = 0;
  while (pos < size) {
    int read = m_data_stream.readRawData(data + pos, size - pos);
    if (read != -1) pos += read;
    if (read == -1 || m_data_stream.atEnd()) {
      // TODO: might not be right for chunked transfer
      qInfo() << "Not enough data has been sent yet." << pos;
      m_data_stream.rollbackTransaction();
      delete[] data;
      return;
    }
  }
  qDebug() << "Received file";
  QList<RemoteActionWithUid> history;
  m_data_stream >> history;
  if (!m_data_stream.commitTransaction()) {
    delete[] data;
    return;
  }
  qDebug() << "Received history of size" << history.size();

  pxtnDescriptor desc;
  desc.set_memory_r(data, size);
  m_ready = true;
  emit connected(desc, history, m_uid);
}
