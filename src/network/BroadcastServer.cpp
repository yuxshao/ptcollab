#include "BroadcastServer.h"

#include <QDataStream>
#include <QDateTime>
#include <QFileInfo>
#include <QMessageBox>
#include <QTcpSocket>
#include <QTimer>

#include "protocol/Hello.h"

const static QString NEXT_UID_KEY("next_uid");
const static double playback_speed = 1;
const static qint64 offset = 0;

constexpr qint64 RECORDING_VERSION = 1;

BroadcastServer::BroadcastServer(std::optional<QString> filename,
                                 QHostAddress host, int port,
                                 std::optional<QString> save_history,
                                 QObject *parent, int delay_msec,
                                 double drop_rate)
    : QObject(parent),
      m_server(new QTcpServer(this)),
      m_sessions(),
      m_next_uid(0),
      m_delay_msec(delay_msec),
      m_drop_rate(drop_rate),
      m_load_history(nullptr),
      m_save_history(nullptr) {
  if (filename.has_value()) {
    QFile *file = new QFile(filename.value(), this);
    if (!file->open(QIODevice::ReadOnly | QIODevice::ExistingOnly))
      throw tr("Could not read file %1").arg(filename.value());

    m_history_elapsed.restart();
    if (QFileInfo(filename.value()).suffix() == "ptrec") {
      m_load_history = std::make_unique<QDataStream>(file);
      qint64 protocol_version, recording_version;
      *m_load_history >> protocol_version >> recording_version;
      if (protocol_version != PROTOCOL_VERSION ||
          recording_version != RECORDING_VERSION)
        throw QString("Incompatible recording version. %1.%2 (%3.%4)")
            .arg(protocol_version)
            .arg(recording_version)
            .arg(PROTOCOL_VERSION)
            .arg(RECORDING_VERSION);
      *m_load_history >> m_next_uid >> m_data;

      m_timer = new QTimer(this);
      m_timer->setSingleShot(true);
      qint64 elapsed;
      *m_load_history >> elapsed;
      m_timer->start(elapsed / playback_speed + offset);
      connect(m_timer, &QTimer::timeout, [this]() {
        qint64 interval = 0;
        while (interval <= 0) {
          if (m_load_history->atEnd()) {
            qWarning() << "Unexpected end of recording";
            return;
          }

          ServerAction a;
          *m_load_history >> a;
          broadcastServerAction(a);

          if (m_load_history->atEnd()) {
            qDebug() << "At end of recording";
            return;
          }

          qint64 next_elapsed;
          *m_load_history >> next_elapsed;
          interval = next_elapsed / playback_speed + offset -
                     m_history_elapsed.elapsed();
        }
        m_timer->start(interval);
      });
    } else {
      m_data = file->readAll();
      file->deleteLater();
    }
  }

  if (save_history.has_value()) {
    QString name = save_history.value() + ".tmp";
    QFile *file = new QFile(name, this);
    m_save_history_filename = save_history.value();
    if (!file->open(QIODevice::ReadWrite))
      throw tr("Unable to open %1 for writing").arg(name);
    m_save_history = std::make_unique<QDataStream>(file);
  }

  if (!m_server->listen(host, port))
    throw QString("Unable to start TCP server: %1")
        .arg(m_server->errorString());
  qInfo() << "Listening on" << m_server->serverAddress()
          << m_server->serverPort();
  connect(m_server, &QTcpServer::newConnection, this,
          &BroadcastServer::newClient);
}

bool BroadcastServer::isReadingHistory() { return m_load_history != nullptr; }

const std::list<AbstractServerSession *> &BroadcastServer::sessions() const {
  return m_sessions;
}

BroadcastServer::~BroadcastServer() {
  // Without these disconnects, I think the sessions' destructors emit the
  // socket disconnected signal which ends up trying to call
  // [broadcastDeleteSession] even after this destructor's been called.
  for (AbstractServerSession *s : m_sessions) s->disconnect(nullptr, this);
  m_server->close();
  finalizeSaveHistory();
}

void BroadcastServer::finalizeSaveHistory() {
  if (!m_save_history) return;
  QFile final_file(m_save_history_filename);
  if (!final_file.open(QIODevice::WriteOnly)) {
    qWarning() << "Cannot open" << m_save_history_filename
               << "for saving recording";
    return;
  }

  QDataStream stream(&final_file);
  stream << PROTOCOL_VERSION << RECORDING_VERSION << m_next_uid << m_data;

  constexpr int SIZE = 4096;
  char buf[SIZE];
  m_save_history->device()->seek(0);
  while (!m_save_history->atEnd()) {
    int read_size = m_save_history->readRawData(buf, SIZE);
    stream.writeRawData(buf, read_size);
  }
  stream.device()->close();
  m_save_history->device()->close();
  QFile(m_save_history_filename + ".tmp").remove();
  qDebug() << "Finalized save history successfully";
}

int BroadcastServer::port() { return m_server->serverPort(); }
QHostAddress BroadcastServer::address() { return m_server->serverAddress(); }

static QMap<qint64, QString> sessionMapping(
    const std::list<AbstractServerSession *> &sessions) {
  QMap<qint64, QString> mapping;
  for (const AbstractServerSession *const s : sessions)
    mapping.insert(s->uid(), s->username());
  return mapping;
}

void BroadcastServer::newClient() {
  QTcpSocket *conn = m_server->nextPendingConnection();
  qInfo() << "New connection" << conn->peerAddress() << m_next_uid;

  registerSession(new ServerSession(this, conn, m_next_uid++));
}

void BroadcastServer::registerSession(AbstractServerSession *session) {
  // It's a bit complicated managing responses to the session in response to
  // its state. Key things to be aware of:
  // 1. Don't send remote actions/edit state until received hello.
  // 2. Clean up properly on disconnect.
  auto deleteOnDisconnect =
      connect(session, &AbstractServerSession::disconnected, session,
              &QObject::deleteLater);
  connect(session, &AbstractServerSession::receivedHello,
          [session, deleteOnDisconnect, this]() {
            broadcastNewSession(session->username(), session->uid());
            m_sessions.push_back(session);

            // Track iterator so we can delete it when it goes away
            auto it = --m_sessions.end();
            disconnect(deleteOnDisconnect);
            connect(session, &AbstractServerSession::disconnected,
                    [it, session, this]() {
                      m_sessions.erase(it);
                      broadcastDeleteSession(session->uid());
                      session->deleteLater();
                    });

            session->sendHello(m_data, m_history, sessionMapping(m_sessions));
            connect(session, &AbstractServerSession::receivedAction, this,
                    &BroadcastServer::broadcastAction);
          });
}

void BroadcastServer::connectLocalSession(LocalClientSession *client,
                                          QString username) {
  qInfo() << "Local connection" << username << m_next_uid;
  LocalServerSession *session =
      new LocalServerSession(this, username, m_next_uid++);
  registerSession(session);
  client->connectToServer(session,
                          HostAndPort{m_server->serverAddress().toString(),
                                      m_server->serverPort()});
}

void BroadcastServer::broadcastServerAction(const ServerAction &a) {
  if (a.shouldBeRecorded())
    qDebug() << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz")
             << "Broadcast to" << m_sessions.size() << to_string(a);
  for (AbstractServerSession *s : m_sessions) s->sendAction(a);
  if (m_save_history) *m_save_history << m_history_elapsed.elapsed() << a;
  if (a.shouldBeRecorded()) m_history.push_back(a);
}

#include <QRandomGenerator>
void BroadcastServer::broadcastUnreliable(const ServerAction &a) {
  if (m_drop_rate > 0 &&
      QRandomGenerator::global()->generateDouble() < m_drop_rate) {
    if (a.shouldBeRecorded())
      qDebug() << QDateTime::currentDateTime().toString(
                      "yyyy.MM.dd hh:mm:ss.zzz")
               << "Dropping from" << m_sessions.size() << to_string(a);
    return;
  }

  if (m_delay_msec > 0)
    QTimer::singleShot(m_delay_msec, [this, a]() { broadcastServerAction(a); });
  else
    broadcastServerAction(a);
}

void BroadcastServer::broadcastAction(const ClientAction &m, qint64 uid) {
  broadcastUnreliable({uid, m});
}

void BroadcastServer::broadcastNewSession(const QString &username, qint64 uid) {
  broadcastServerAction({uid, NewSession{username}});
}

void BroadcastServer::broadcastDeleteSession(qint64 uid) {
  broadcastServerAction({uid, DeleteSession{}});
}
