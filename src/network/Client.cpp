#include "Client.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QHostAddress>
#include <QMessageBox>

#include "protocol/Hello.h"

QString HostAndPort::toString() { return QString("%1:%2").arg(host).arg(port); }

Client::Client(QObject *parent)
    : QObject(parent),
      m_socket(new QTcpSocket(this)),
      m_local(nullptr),
      m_write_stream((QIODevice *)m_socket),
      m_read_stream((QIODevice *)m_socket),
      m_received_hello(false) {
  connect(m_socket, &QTcpSocket::readyRead, this, &Client::tryToRead);
  connect(m_socket, &QTcpSocket::disconnected, this,
          &Client::handleExternalDisconnect);

  connect(m_socket, &QTcpSocket::errorOccurred,
          [this](QAbstractSocket::SocketError) {
            emit errorOccurred(m_socket->errorString());
          });

  m_write_stream.setVersion(QDataStream::Qt_5_5);
  m_read_stream.setVersion(QDataStream::Qt_5_5);
}

void Client::handleExternalDisconnect() {
  if (m_local != nullptr) m_local = nullptr;
  m_received_hello = false;
  emit disconnected(m_suppress_disconnect);
  m_suppress_disconnect = false;
}

HostAndPort Client::currentlyConnectedTo() {
  if (m_local)
    return m_local->connectedTo();
  else
    return HostAndPort{m_socket->peerAddress().toString(),
                       m_socket->peerPort()};
}

void Client::connectToServer(QString hostname, quint16 port, QString username) {
  if (m_local != nullptr) {
    m_local->clientDisconnect();
    m_local = nullptr;
  }
  m_socket->abort();

  // Guarded on connection in case the connection fails. In the past not having
  // this has caused me problems
  QMetaObject::Connection *const conn = new QMetaObject::Connection;
  *conn = connect(m_socket, &QTcpSocket::connected, [this, conn, username]() {
    qDebug() << "Sending hello to server";
    m_write_stream << ClientHello(username);
    disconnect(*conn);
    delete conn;
  });

  m_socket->connectToHost(hostname, port);
}

void Client::connectToLocalServer(BroadcastServer *server, QString username) {
  m_local = server->makeLocalSession(username);
  connect(m_local, &QObject::destroyed, [this](QObject *) {
    if (m_local != nullptr) handleExternalDisconnect();
  });
  connect(m_local, &LocalServerSession::clientReceivedHello,
          [this](const QByteArray &file, const QList<ServerAction> &history,
                 const QMap<qint64, QString> &) {
            connected(file, history, m_local->uid());
          });
  connect(m_local, &LocalServerSession::clientReceivedAction, this,
          &Client::receivedAction);
  m_local->clientSendHello();
}

void Client::disconnectFromServerSuppressSignal() {
  if (m_local != nullptr) {
    m_local->clientDisconnect();
    m_local = nullptr;
  }
  if (m_socket->state() == QAbstractSocket::ConnectedState) {
    m_suppress_disconnect = true;
    m_socket->disconnectFromHost();
  }
}

void Client::sendAction(const ClientAction &m) {
  if (clientActionShouldBeRecorded(m))
    qDebug() << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz")
             << "Sending" << m;
  if (m_local != nullptr)
    m_local->clientSendAction(m);
  else {
    // Sometimes if I try to write data to a socket that's not ready it
    // invalidates the socket forever. I think these two guards should prevent
    // it.
    if (m_socket->isValid() &&
        m_socket->state() == QTcpSocket::ConnectedState) {
      m_write_stream << m;
      if (m_socket->bytesToWrite() == 0) {
        qWarning() << "Client::sendAction didn't seem to fill write." << m;
        qWarning() << "Socket state: open(" << m_socket->isOpen()
                   << "), valid (" << m_socket->isValid() << "), state("
                   << m_socket->state() << "), error("
                   << m_socket->errorString() << ")"
                   << "), stream_status(" << m_write_stream.status() << ")";
      }
    } else {
      /*qDebug() << "Not sending action while socket is not ready" << m;
      qDebug() << "Socket state: open(" << m_socket->isOpen() << "), valid ("
               << m_socket->isValid() << "), state(" << m_socket->state()
               << "), error(" << m_socket->errorString() << ")";*/
    }
  }
}

qint64 Client::uid() { return m_uid; }

void Client::tryToRead() {
  // qDebug() << "Client has bytes available" << m_socket->bytesAvailable();
  // I got tripped up. tryToStart cannot be in the loop b/c that will cause it
  // to loop infinitely. The way it's coded right now at least.
  if (!m_received_hello) tryToStart();
  if (m_received_hello)
    while (!m_read_stream.atEnd()) {
      m_read_stream.startTransaction();

      ServerAction action;
      try {
        m_read_stream >> action;
      } catch (const std::runtime_error &e) {
        qWarning("Discarding unreadable server action. Error: %s. ", e.what());
        m_read_stream.rollbackTransaction();
        return;
      }
      if (!m_read_stream.commitTransaction()) {
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

  m_read_stream.startTransaction();
  m_read_stream >> hello >> data >> history >> sessions;
  if (!m_read_stream.commitTransaction()) return;

  if (!hello.isValid()) {
    qWarning("Invalid hello response. Disconnecting.");
    emit errorOccurred(
        tr("Invalid hello response from server. Disconnecting."));
    m_socket->disconnectFromHost();
    return;
  }
  m_uid = hello.uid();

  qDebug() << "Received uid" << m_uid << "and history of size"
           << history.size();

  m_received_hello = true;
  emit connected(data, history, m_uid);
  // for (auto it = sessions.begin(); it != sessions.end(); ++it)
  //  emit receivedNewSession(it.value(), it.key());
}
