#include "BroadcastServer.h"

#include <QDataStream>
#include <QDateTime>
#include <QMessageBox>
#include <QTcpSocket>
#include <QTimer>

const static QString NEXT_UID_KEY("next_uid");

BroadcastServer::BroadcastServer(const QByteArray &data, int port,
                                 QString load_history, QString save_history,
                                 QObject *parent, int delay_msec,
                                 double drop_rate)
    : QObject(parent),
      m_server(new QTcpServer(this)),
      m_sessions(),
      m_data(data),
      m_next_uid(0),
      m_delay_msec(delay_msec),
      m_drop_rate(drop_rate),
      m_load_history(nullptr),
      m_save_history(nullptr),
      m_save_history_metadata(nullptr) {
  if (!m_server->listen(QHostAddress::Any, port))
    throw QString("Unable to start TCP server: %1")
        .arg(m_server->errorString());

  qInfo() << "Listening on" << m_server->serverPort();

  m_history_elapsed.restart();

  if (load_history != "") {
    QFile *file = new QFile(load_history, this);
    if (!file->open(QIODevice::ReadOnly | QIODevice::ExistingOnly))
      throw QString("Unable to load history");
    m_load_history = std::make_unique<QDataStream>(file);
    *m_load_history >> m_data;
    QSettings metadata(load_history + "-meta", QSettings::IniFormat);
    bool ok;
    m_next_uid = metadata.value(NEXT_UID_KEY, -1).toInt(&ok);
    if (m_next_uid == -1 || !ok)
      throw QString("Unable to load history metadata");

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    qint64 elapsed;
    *m_load_history >> elapsed;
    qDebug() << "Waiting" << elapsed;
    m_timer->start(elapsed);
    connect(m_timer, &QTimer::timeout, [this]() {
      ServerAction a;
      *m_load_history >> a;
      broadcastServerAction(a);

      if (m_load_history->atEnd()) {
        qDebug() << "At end of recording";
        return;
      }

      qint64 next_elapsed;
      *m_load_history >> next_elapsed;
      m_timer->start(std::max(0LL, next_elapsed - m_history_elapsed.elapsed()));
    });
  }

  if (save_history != "") {
    QFile *file = new QFile(save_history, this);
    if (!file->open(QIODevice::WriteOnly))
      throw QString("Unable to open history file for saving");
    m_save_history = std::make_unique<QDataStream>(file);
    *m_save_history << m_data;
    m_save_history_metadata = std::make_unique<QSettings>(
        save_history + "-meta", QSettings::IniFormat);
  }

  connect(m_server, &QTcpServer::newConnection, this,
          &BroadcastServer::newClient);
}

BroadcastServer::~BroadcastServer() {
  // Without these disconnects, I think the sessions' destructors emit the
  // socket disconnected signal which ends up trying to call
  // [broadcastDeleteSession] even after this destructor's been called.
  for (ServerSession *s : m_sessions) s->disconnect();
  m_server->close();
  if (m_save_history_metadata) {
    m_save_history_metadata->setValue("next_uid", m_next_uid);
    m_save_history_metadata->sync();
  }
}

int BroadcastServer::port() { return m_server->serverPort(); }

static QMap<qint64, QString> sessionMapping(
    const std::list<ServerSession *> &sessions) {
  QMap<qint64, QString> mapping;
  for (const ServerSession *const s : sessions)
    mapping.insert(s->uid(), s->username());
  return mapping;
}

void BroadcastServer::newClient() {
  QTcpSocket *conn = m_server->nextPendingConnection();
  qInfo() << "New connection" << conn->peerAddress();

  ServerSession *session = new ServerSession(this, conn, m_next_uid++);

  // It's a bit complicated managing responses to the session in response to
  // its state. Key things to be aware of:
  // 1. Don't send remote actions/edit state until received hello.
  // 2. Clean up properly on disconnect.
  auto deleteOnDisconnect = connect(session, &ServerSession::disconnected,
                                    session, &QObject::deleteLater);
  connect(session, &ServerSession::receivedHello,
          [session, deleteOnDisconnect, this]() {
            broadcastNewSession(session->username(), session->uid());
            m_sessions.push_back(session);

            // Track iterator so we can delete it when it goes away
            auto it = --m_sessions.end();
            disconnect(deleteOnDisconnect);
            connect(session, &ServerSession::disconnected,
                    [it, session, this]() {
                      m_sessions.erase(it);
                      broadcastDeleteSession(session->uid());
                      session->deleteLater();
                    });

            session->sendHello(m_data, m_history, sessionMapping(m_sessions));
            connect(session, &ServerSession::receivedAction, this,
                    &BroadcastServer::broadcastAction);
          });
}

void BroadcastServer::broadcastServerAction(const ServerAction &a) {
  if (a.shouldBeRecorded())
    qDebug() << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz")
             << "Broadcast to" << m_sessions.size() << a;
  for (ServerSession *s : m_sessions) s->sendAction(a);
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
               << "Dropping from" << m_sessions.size() << a;
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
