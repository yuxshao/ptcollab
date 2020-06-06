#include "ActionClient.h"

ActionClient::ActionClient(QObject *parent, QString hostname, quint16 port)
    : QObject(parent),
      m_socket(new QTcpSocket(this)),
      m_data_stream((QIODevice *)m_socket),
      m_ready(false) {
  m_socket->connectToHost(hostname, port);
  connect(m_socket, &QTcpSocket::readyRead, this, &ActionClient::tryToRead);
}

void ActionClient::sendRemoteAction(const RemoteAction &action) {
  m_data_stream << action;
}

int ActionClient::uid() { return m_uid; }

void ActionClient::tryToRead() {
  if (!m_ready) tryToStart();
  while (!m_data_stream.atEnd()) {
    m_data_stream.startTransaction();
    qint64 uid;
    RemoteAction action;
    m_data_stream >> uid >> action;
    if (!m_data_stream.commitTransaction()) return;
    emit receivedRemoteAction(uid, action);
  }
}

void ActionClient::tryToStart() {
  // send over the file + whole history
  qInfo() << "Getting initial data from server";
  QString header;
  qint32 version;
  m_data_stream.startTransaction();
  m_data_stream >> header >> version;
  if (header != "PXTONE_HISTORY") qFatal("Unexpected header");
  if (version != 1) qFatal("Unexpected version");
  m_data_stream.setVersion(QDataStream::Qt_5_14);

  m_data_stream >> m_uid;

  qint64 size;
  m_data_stream >> size;
  char *data = new char[size];
  if (m_data_stream.readRawData(data, size) < size) {
    // TODO: might not be right for chunked transfer
    qInfo() << "Not enough data has been sent yet.";
    m_data_stream.abortTransaction();
    delete[] data;
    return;
  }

  QList<RemoteAction> history;
  m_data_stream >> history;
  if (!m_data_stream.commitTransaction()) {
    delete[] data;
    return;
  }

  pxtnDescriptor desc;
  desc.set_memory_r(data, size);
  m_ready = true;
  emit ready(desc, history);
}
