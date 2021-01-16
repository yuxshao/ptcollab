#include "PxtoneClient.h"

#include <QMessageBox>
#include <QSettings>
#include <QTimer>

#include "ComboOptions.h"
#include "Settings.h"
#include "audio/AudioFormat.h"

QList<UserListEntry> getUserList(
    const std::map<qint64, RemoteEditState> &users) {
  QList<UserListEntry> list;
  for (auto it = users.begin(); it != users.end(); ++it)
    list.append(
        UserListEntry{it->first, it->second.last_ping, it->second.user});
  return list;
}

const static int PING_INTERVAL = 2000;

PxtoneClient::PxtoneClient(pxtnService *pxtn,
                           ConnectionStatusLabel *connection_status,
                           QObject *parent)
    : QObject(parent),
      m_controller(new PxtoneController(0, pxtn, &m_moo_state, this)),
      m_client(new Client(this)),
      m_following_user(std::nullopt),
      m_ping_timer(new QTimer(this)),
      m_last_seek(0),
      m_clipboard(new Clipboard(this)) {
  QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
  if (!info.isFormatSupported(pxtoneAudioFormat())) {
    qWarning()
        << "Raw audio format not supported by backend, cannot play audio.";
    return;
  }
  m_pxtn_device = new PxtoneIODevice(this, m_controller->pxtn(), &m_moo_state);
  m_audio = new QAudioOutput(pxtoneAudioFormat(), this);

  // Apparently this reduces latency in pulseaudio, but also makes
  // some sounds choppier
  // m_audio->setCategory("game");
  m_audio->setVolume(1.0);
  connect(m_pxtn_device, &PxtoneIODevice::playingChanged, this,
          &PxtoneClient::playStateChanged);

  connect(m_ping_timer, &QTimer::timeout, [this]() {
    sendPlayState(false);
    sendAction(Ping{QDateTime::currentMSecsSinceEpoch(), m_last_ping});
  });

  connect(
      m_client, &Client::connected,
      [this, connection_status](pxtnDescriptor &desc,
                                QList<ServerAction> &history, qint64 uid) {
        HostAndPort host_and_port = m_client->currentlyConnectedTo();
        connection_status->setClientConnectionState(host_and_port.toString());
        qDebug() << "Connected to server" << host_and_port.toString();
        loadDescriptor(desc);
        emit connected();
        m_controller->setUid(uid);
        for (ServerAction &a : history) processRemoteAction(a);
        sendAction(Ping{QDateTime::currentMSecsSinceEpoch(), m_last_ping});
        m_ping_timer->start(PING_INTERVAL);
      });
  connect(m_client, &Client::disconnected,
          [this, connection_status](bool suppress_alert) {
            connection_status->setClientConnectionState(std::nullopt);
            emit beginUserListRefresh();
            m_remote_edit_states.clear();
            emit endUserListRefresh();
            if (!suppress_alert)
              QMessageBox::information(nullptr, "Disconnected",
                                       "Disconnected from server.");
            m_last_ping = std::nullopt;
            updatePing(m_last_ping);
          });
  connect(m_client, &Client::errorOccurred, [](QString error) {
    QMessageBox::information(nullptr, "Connection error",
                             tr("Connection error: %1").arg(error));
  });
  connect(m_client, &Client::receivedAction, this,
          &PxtoneClient::processRemoteAction);
}

void PxtoneClient::loadDescriptor(pxtnDescriptor &desc) {
  // An empty desc is interpreted as an empty file so we don't error.
  m_controller->loadDescriptor(desc);
  changeEditState([](EditState &e) { e.m_current_unit_id = 0; }, false);
  m_following_user.reset();
  m_pxtn_device->setPlaying(false);
  seekMoo(0);

  m_pxtn_device->open(QIODevice::ReadOnly);
  {
    bool ok;
    int v = QSettings().value(VOLUME_KEY).toInt(&ok);
    if (ok) setVolume(v);
  }
  {
    bool ok;
    double v = QSettings()
                   .value(BUFFER_LENGTH_KEY, DEFAULT_BUFFER_LENGTH)
                   .toDouble(&ok);
    if (ok) setBufferSize(v);
  }
  m_pxtn_device->setPlaying(false);
  m_audio->start(m_pxtn_device);
  qDebug() << "Actual" << m_audio->bufferSize();
}

void PxtoneClient::setBufferSize(double secs) {
  bool started = (m_audio->state() != QAudio::StoppedState &&
                  m_audio->state() != QAudio::IdleState);
  QAudioFormat fmt = pxtoneAudioFormat();

  if (started) {
    m_audio->stop();
    m_pxtn_device->setPlaying(false);
  }

  if (secs < 0.01) secs = 0.01;
  if (secs > 10) secs = 10;
  qDebug() << "Setting buffer size: " << secs;
  m_audio->setBufferSize(fmt.bytesForDuration(secs * 1e6));

  if (started) m_audio->start(m_pxtn_device);
}

bool PxtoneClient::isPlaying() { return m_pxtn_device->playing(); }

// TODO: Factor this out into a PxtoneAudioPlayer class. Setting play state,
// seeking. Unfortunately start / stop don't even fix this because stopping
// still waits for the buffer to drain (instead of flushing and throwing away)
void PxtoneClient::togglePlayState() {
  if (SpacebarStop::get() && m_pxtn_device->playing())
    resetAndSuspendAudio();
  else {
    m_pxtn_device->setPlaying(!m_pxtn_device->playing());
    sendPlayState(true);
  }
}

void PxtoneClient::sendPlayState(bool from_action) {
  sendAction(PlayState{pxtn()->moo_get_now_clock(*moo()),
                       m_pxtn_device->playing(), from_action});
}

void PxtoneClient::resetAndSuspendAudio() {
  m_pxtn_device->setPlaying(false);
  if (pxtn()->moo_get_now_clock(*moo()) > m_last_seek)
    seekMoo(m_last_seek);
  else
    seekMoo(0);
}

void PxtoneClient::setFollowing(std::optional<qint64> following) {
  m_following_user = following;

  if (following.has_value() && following != m_controller->uid()) {
    sendAction(WatchUser{following.value()});
    const auto it = m_remote_edit_states.find(following.value());
    if (it != m_remote_edit_states.end() && it->second.state.has_value()) {
      // stop audio, so that the next playstate msg causes us to sync if the
      // other user's playing.
      m_pxtn_device->setPlaying(false);
      emit followActivity(it->second.state.value());
    }
  }
}

bool PxtoneClient::isFollowing() {
  return (m_following_user.has_value() &&
          m_following_user.value() != m_controller->uid());
}

qint64 PxtoneClient::uid() const { return m_controller->uid(); }
qint64 PxtoneClient::following_uid() const {
  if (m_following_user.has_value()) return m_following_user.value();
  return m_controller->uid();
}

qint32 PxtoneClient::quantizeClock(int idx) {
  return pxtn()->master->get_beat_clock() / idx;
}

qint32 PxtoneClient::quantizeClock() {
  return quantizeClock(
      quantizeXOptions[editState().m_quantize_clock_idx].second);
}

qint32 PxtoneClient::quantizePitch(int idx) { return PITCH_PER_KEY / idx; }

qint32 PxtoneClient::quantizePitch() {
  return quantizePitch(
      quantizeYOptions[editState().m_quantize_pitch_idx].second);
}

qint32 PxtoneClient::lastSeek() const { return m_last_seek; }

void PxtoneClient::applyAction(const std::list<Action::Primitive> &as) {
  m_client->sendAction(m_controller->applyLocalAction(as));
}

void PxtoneClient::sendAction(const ClientAction &a) {
  m_client->sendAction(a);
}

void PxtoneClient::removeCurrentUnit() {
  if (unitIdMap().numUnits() > 0)
    m_client->sendAction(RemoveUnit{m_edit_state.m_current_unit_id});
}

void PxtoneClient::seekMoo(int64_t clock) {
  m_last_seek = clock;
  m_controller->seekMoo(clock);
  sendPlayState(true);
}

void PxtoneClient::connectToServer(QString hostname, quint16 port,
                                   QString username) {
  m_client->connectToServer(hostname, port, username);
}

void PxtoneClient::disconnectFromServerSuppressSignal() {
  m_client->disconnectFromServerSuppressSignal();
}

void PxtoneClient::changeEditState(std::function<void(EditState &)> f,
                                   bool preserveFollow) {
  f(m_edit_state);
  if (!preserveFollow) setFollowing(std::nullopt);
  if (!m_following_user.has_value() ||
      m_following_user.value() == m_controller->uid())
    m_client->sendAction(m_edit_state);
  emit editStateChanged(m_edit_state);
}

void PxtoneClient::processRemoteAction(const ServerAction &a) {
  qint64 uid = a.uid;
  std::visit(
      overloaded{
          [this, uid](const ClientAction &s) {
            std::visit(
                overloaded{
                    [this, uid](const EditState &s) {
                      auto it = m_remote_edit_states.find(uid);
                      if (it == m_remote_edit_states.end())
                        qWarning()
                            << "Received edit state for unknown session" << uid;
                      else {
                        it->second.state.emplace(s);
                        if (uid != m_controller->uid() &&
                            m_following_user == uid)
                          emit followActivity(s);
                      }
                    },
                    [this, uid](const WatchUser &) {
                      // TODO: Maybe remember who's watching whom so that if you
                      // watch them you follow their watchee too?
                      auto it = m_remote_edit_states.find(uid);
                      if (it == m_remote_edit_states.end())
                        qWarning()
                            << "Received watch user for unknown session" << uid;
                      it->second.state.reset();
                    },
                    [this, uid](const Ping &s) {
                      auto it = m_remote_edit_states.find(uid);
                      if (it == m_remote_edit_states.end())
                        qWarning()
                            << "Received ping for unknown session" << uid;
                      else
                        it->second.last_ping = s.last_ping_length;
                      if (uid == m_controller->uid()) {
                        m_last_ping =
                            QDateTime::currentMSecsSinceEpoch() - s.now;
                        updatePing(m_last_ping);
                      }
                    },
                    [this, uid](const PlayState &s) {
                      if (uid == following_uid() &&
                          uid != m_controller->uid()) {
                        // Since playstates come with heartbeats, we only reset
                        // the moo for non-heartbeat updates or if there's a
                        // drastic diff.
                        if (m_pxtn_device->playing() != s.playing ||
                            s.from_action)
                          m_controller->seekMoo(s.clock);
                        m_pxtn_device->setPlaying(s.playing);
                      }
                    },
                    [this, uid](const EditAction &s) {
                      m_controller->applyRemoteAction(s, uid);
                    },
                    [this, uid](const UndoRedo &s) {
                      m_controller->applyUndoRedo(s, uid);
                    },
                    [this, uid](const TempoChange &s) {
                      m_controller->applyTempoChange(s, uid);
                    },
                    [this, uid](const BeatChange &s) {
                      m_controller->applyBeatChange(s, uid);
                    },
                    [this, uid](const SetRepeatMeas &s) {
                      m_controller->applySetRepeatMeas(s, uid);
                    },
                    [this, uid](const SetLastMeas &s) {
                      m_controller->applySetLastMeas(s, uid);
                    },
                    [this, uid](const Overdrive::Add &s) {
                      m_controller->applyAddOverdrive(s, uid);
                    },
                    [this, uid](const Overdrive::Set &s) {
                      m_controller->applySetOverdrive(s, uid);
                    },
                    [this, uid](const Overdrive::Remove &s) {
                      m_controller->applyRemoveOverdrive(s, uid);
                    },
                    [this, uid](const Delay::Set &s) {
                      m_controller->applySetDelay(s, uid);
                    },
                    [this, uid](const AddWoice &s) {
                      bool success = m_controller->applyAddWoice(s, uid);
                      if (!success) {
                        qWarning() << "Could not add voice" << s.name
                                   << "from user" << uid;
                        if (uid == m_controller->uid())
                          QMessageBox::warning(
                              nullptr, tr("Could not add voice"),
                              tr("Could not add voice %1").arg(s.name));
                      }
                    },
                    [this, uid](const RemoveWoice &s) {
                      bool success = m_controller->applyRemoveWoice(s, uid);
                      if (!success && uid == m_controller->uid())
                        QMessageBox::warning(
                            nullptr, tr("Could not remove voice"),
                            tr("Could not add remove %1").arg(s.name));
                      else
                        // Refresh units using the woice.
                        // (No refresh is also fine but just stale)
                        m_controller->refreshMoo();
                    },
                    [this, uid](const ChangeWoice &s) {
                      bool success = m_controller->applyChangeWoice(s, uid);
                      if (!success && uid == m_controller->uid())
                        QMessageBox::warning(
                            nullptr, tr("Could not change voice"),
                            tr("Could not add change %1").arg(s.remove.name));
                      else
                        m_controller->refreshMoo();
                    },
                    [this, uid](const Woice::Set &s) {
                      bool success = m_controller->applyWoiceSet(s, uid);
                      if (!success && uid == m_controller->uid())
                        QMessageBox::warning(
                            nullptr, tr("Could not change voice"),
                            tr("Could not change voice properties"));
                    },
                    [this, uid](const AddUnit &s) {
                      bool success = m_controller->applyAddUnit(s, uid);
                      changeEditState(
                          [&](EditState &s) {
                            if (uid == m_controller->uid() && success) {
                              const NoIdMap &unitIdMap =
                                  m_controller->unitIdMap();
                              s.m_current_unit_id =
                                  unitIdMap.noToId(unitIdMap.numUnits() - 1);
                            }
                          },
                          true);
                    },
                    [this, uid](const SetUnitName &s) {
                      m_controller->applySetUnitName(s, uid);
                    },
                    [this, uid](const MoveUnit &s) {
                      m_controller->applyMoveUnit(s, uid);
                    },
                    [this, uid](const RemoveUnit &s) {
                      auto unit_no = m_controller->unitIdMap().idToNo(
                          m_edit_state.m_current_unit_id);
                      m_controller->applyRemoveUnit(s, uid);
                      changeEditState(
                          [&](EditState &s) {
                            if (unit_no != std::nullopt) {
                              const NoIdMap &unitIdMap =
                                  m_controller->unitIdMap();
                              if (unitIdMap.numUnits() <=
                                      size_t(unit_no.value()) &&
                                  unitIdMap.numUnits() > 0) {
                                s.m_current_unit_id =
                                    unitIdMap.noToId(unit_no.value() - 1);
                              }
                            }
                          },
                          true);
                    }},
                s);
          },
          [this, uid](const NewSession &s) {
            bool overwriting =
                (m_remote_edit_states.find(uid) != m_remote_edit_states.end());
            if (overwriting)
              qWarning() << "Remote session already exists for uid" << uid
                         << "; overwriting";
            else {
              auto pos = m_remote_edit_states.lower_bound(uid);
              emit beginAddUser(
                  std::distance(m_remote_edit_states.begin(), pos));
            }
            m_remote_edit_states[uid] =
                RemoteEditState{std::nullopt, std::nullopt, s.username};
            if (!overwriting) emit endAddUser();
          },
          [this, uid](const DeleteSession &s) {
            (void)s;
            auto pos = m_remote_edit_states.find(uid);
            if (pos == m_remote_edit_states.end()) {
              qWarning() << "Received delete for unknown remote session";
              return;
            }
            emit beginRemoveUser(
                std::distance(m_remote_edit_states.begin(), pos));
            m_remote_edit_states.erase(pos);
            if (following_uid() == uid) setFollowing(std::nullopt);
            emit endRemoveUser();
          },
      },
      a.action);
}

void PxtoneClient::setCurrentUnitNo(int unit_no, bool preserveFollow) {
  // Unfortunately, we need this check because it's hard to distinguish to the
  // caller if this was called because the followee chaned unit.
  int unit_id = m_controller->unitIdMap().noToId(unit_no);
  if (editState().m_current_unit_id != unit_id)
    changeEditState([&](EditState &s) { s.m_current_unit_id = unit_id; },
                    preserveFollow);
}

void PxtoneClient::setCurrentWoiceNo(int woice_no, bool preserveFollow) {
  changeEditState(
      [&](EditState &s) {
        s.m_current_woice_id = m_controller->woiceIdMap().noToId(woice_no);
      },
      preserveFollow);
}

void PxtoneClient::setVolume(int volume) { m_controller->setVolume(volume); }

void PxtoneClient::deselect(bool preserveFollow) {
  changeEditState([&](auto &s) { s.mouse_edit_state.selection.reset(); },
                  preserveFollow);
}

void PxtoneClient::setUnitPlayed(int unit_no, bool played) {
  m_controller->setUnitPlayed(unit_no, played);
}
void PxtoneClient::setUnitVisible(int unit_no, bool visible) {
  m_controller->setUnitVisible(unit_no, visible);
}
void PxtoneClient::setUnitOperated(int unit_no, bool operated) {
  m_controller->setUnitOperated(unit_no, operated);
}
void PxtoneClient::toggleSolo(int unit_no) {
  m_controller->toggleSolo(unit_no);
}
