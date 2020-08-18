#include "PxtoneClient.h"

#include <QMessageBox>
#include <QSettings>

#include "ComboOptions.h"
#include "Settings.h"
#include "audio/AudioFormat.h"

QList<std::pair<qint64, QString>> getUserList(
    const std::unordered_map<qint64, RemoteEditState> &users) {
  QList<std::pair<qint64, QString>> list;
  for (auto it = users.begin(); it != users.end(); ++it)
    list.append(std::make_pair(it->first, it->second.user));
  return list;
}

PxtoneClient::PxtoneClient(pxtnService *pxtn, QLabel *client_status,
                           QObject *parent)
    : QObject(parent),
      m_controller(new PxtoneController(0, pxtn, &m_moo_state, this)),
      m_client(new Client(this)),
      m_clipboard(new Clipboard(pxtn, this)) {
  QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
  if (!info.isFormatSupported(pxtoneAudioFormat())) {
    qWarning()
        << "Raw audio format not supported by backend, cannot play audio.";
    return;
  }
  m_pxtn_device = new PxtoneIODevice(this, m_controller->pxtn(), &m_moo_state);
  m_audio = new QAudioOutput(pxtoneAudioFormat(), this);

  // m_audio->setBufferSize(441000);

  // Apparently this reduces latency in pulseaudio, but also makes
  // some sounds choppier
  // m_audio->setCategory("game");
  m_audio->setVolume(1.0);
  connect(m_audio, &QAudioOutput::stateChanged, [this](QAudio::State state) {
    switch (state) {
      case QAudio::ActiveState:
        emit playStateChanged(true);
        break;
      case QAudio::SuspendedState:
        emit playStateChanged(false);
        break;
      case QAudio::IdleState:
      case QAudio::InterruptedState:
      case QAudio::StoppedState:
        qWarning() << "Entered unexpected play state" << state;
        emit playStateChanged(false);
        break;
    }
  });

  connect(m_client, &Client::connected,
          [this, client_status](pxtnDescriptor &desc,
                                QList<ServerAction> &history, qint64 uid) {
            HostAndPort host_and_port = m_client->currentlyConnectedTo();
            client_status->setText(tr("Connected to %1:%2")
                                       .arg(host_and_port.host)
                                       .arg(host_and_port.port));
            QMessageBox::information(nullptr, "Connected",
                                     "Connected to server.");
            loadDescriptor(desc);
            emit connected();
            m_controller->setUid(uid);
            for (ServerAction &a : history) processRemoteAction(a);
          });
  connect(m_client, &Client::disconnected, [this, client_status]() {
    client_status->setText(tr("Not connected"));
    m_remote_edit_states.clear();
    emit userListChanged(getUserList(m_remote_edit_states));
    QMessageBox::information(nullptr, "Disconnected",
                             "Disconnected from server.");
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

  resetAndSuspendAudio();
  m_pxtn_device->open(QIODevice::ReadOnly);
  {
    bool ok;
    int v = QSettings().value(VOLUME_KEY).toInt(&ok);
    if (ok) setVolume(v);
  }
  {
    bool ok;
    int v = QSettings().value(BUFFER_LENGTH_KEY).toDouble(&ok);
    if (ok) setBufferSize(v);
  }
  m_audio->start(m_pxtn_device);
  qDebug() << "Actual" << m_audio->bufferSize();
  m_audio->suspend();
}

void PxtoneClient::setBufferSize(double secs) {
  bool started = (m_audio->state() != QAudio::StoppedState);
  QAudioFormat fmt = pxtoneAudioFormat();

  if (started) {
    m_audio->stop();
  }

  m_audio->setBufferSize(fmt.bytesForDuration(secs * 1e6));

  if (started) {
    resetAndSuspendAudio();
    m_audio->start(m_pxtn_device);
    m_audio->suspend();
  }
}

// TODO: Factor this out into a PxtoneAudioPlayer class. Setting play state,
// seeking. Unfortunately start / stop don't even fix this because stopping
// still waits for the buffer to drain (instead of flushing and throwing away)
void PxtoneClient::togglePlayState() {
  switch (m_audio->state()) {
    case QAudio::ActiveState:
      m_audio->suspend();
      break;
    case QAudio::SuspendedState:
      m_audio->resume();
      break;
    case QAudio::IdleState:
    case QAudio::InterruptedState:
    case QAudio::StoppedState:
      qWarning() << "Toggling play state of audio that is not playing"
                 << m_audio->state();
      break;
  }
}

void PxtoneClient::resetAndSuspendAudio() {
  seekMoo(0);
  m_audio->suspend();
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

void PxtoneClient::seekMoo(int64_t clock) { m_controller->seekMoo(clock); }

void PxtoneClient::connectToServer(QString hostname, quint16 port,
                                   QString username) {
  m_client->connectToServer(hostname, port, username);
}

void PxtoneClient::changeEditState(std::function<void(EditState &)> f) {
  f(m_edit_state);
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
                      else
                        it->second.state.emplace(s);
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
                      changeEditState([&](EditState &s) {
                        if (uid == m_controller->uid() && success) {
                          const NoIdMap &unitIdMap = m_controller->unitIdMap();
                          s.m_current_unit_id =
                              unitIdMap.noToId(unitIdMap.numUnits() - 1);
                        }
                      });
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
                      changeEditState([&](EditState &s) {
                        if (unit_no != std::nullopt) {
                          const NoIdMap &unitIdMap = m_controller->unitIdMap();
                          if (unitIdMap.numUnits() <= size_t(unit_no.value()) &&
                              unitIdMap.numUnits() > 0) {
                            s.m_current_unit_id =
                                unitIdMap.noToId(unit_no.value() - 1);
                          }
                        }
                      });
                    }},
                s);
          },
          [this, uid](const NewSession &s) {
            if (m_remote_edit_states.find(uid) != m_remote_edit_states.end())
              qWarning() << "Remote session already exists for uid" << uid
                         << "; overwriting";
            m_remote_edit_states[uid] =
                RemoteEditState{std::nullopt, s.username};
            emit userListChanged(getUserList(m_remote_edit_states));
          },
          [this, uid](const DeleteSession &s) {
            (void)s;
            if (!m_remote_edit_states.erase(uid))
              qWarning() << "Received delete for unknown remote session";
            emit userListChanged(getUserList(m_remote_edit_states));
          },
      },
      a.action);
}

void PxtoneClient::setCurrentUnitNo(int unit_no) {
  changeEditState([&](EditState &s) {
    s.m_current_unit_id = m_controller->unitIdMap().noToId(unit_no);
  });
}

void PxtoneClient::setCurrentWoiceNo(int woice_no) {
  changeEditState([&](EditState &s) {
    s.m_current_woice_id = m_controller->woiceIdMap().noToId(woice_no);
  });
}

void PxtoneClient::setVolume(int volume) { m_controller->setVolume(volume); }

void PxtoneClient::deselect() {
  changeEditState([&](auto &s) { s.mouse_edit_state.selection.reset(); });
}
