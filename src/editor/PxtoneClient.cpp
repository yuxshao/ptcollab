#include "PxtoneClient.h"

#include <QMessageBox>

#include "AudioFormat.h"

QList<std::pair<qint64, QString>> getUserList(
    const std::unordered_map<qint64, RemoteEditState> &users) {
  QList<std::pair<qint64, QString>> list;
  for (auto it = users.begin(); it != users.end(); ++it)
    list.append(std::make_pair(it->first, it->second.user));
  return list;
}

PxtoneClient::PxtoneClient(pxtnService *pxtn, QLabel *client_status,
                           UnitListModel *units, QObject *parent)
    : QObject(parent),
      m_controller(new PxtoneController(0, pxtn, &m_moo_state, this)),
      m_client(new Client(this)),
      m_units(units) {
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
  connect(m_controller, &PxtoneController::measureNumChanged, this,
          &PxtoneClient::measureNumChanged);
  connect(m_controller, &PxtoneController::edited, this, &PxtoneClient::edited);
}

void PxtoneClient::loadDescriptor(pxtnDescriptor &desc) {
  // An empty desc is interpreted as an empty file so we don't error.
  // m_units should be in controller
  m_units->beginRefresh();
  m_controller->loadDescriptor(desc);
  m_units->endRefresh();
  emit woicesChanged();
  emit tempoBeatChanged();

  resetAndSuspendAudio();
  m_pxtn_device->open(QIODevice::ReadOnly);
  m_audio->start(m_pxtn_device);
  m_audio->suspend();
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
  m_controller->seekMoo(clock);
  emit seeked(clock);
}

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
                      if (m_controller->applyTempoChange(s, uid))
                        emit tempoBeatChanged();
                    },
                    [this, uid](const BeatChange &s) {
                      if (m_controller->applyBeatChange(s, uid))
                        emit tempoBeatChanged();
                    },
                    [this, uid](const AddWoice &s) {
                      bool success = m_controller->applyAddWoice(s, uid);
                      emit woicesChanged();
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
                      emit woicesChanged();
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
                      emit woicesChanged();
                      if (!success && uid == m_controller->uid())
                        QMessageBox::warning(
                            nullptr, tr("Could not change voice"),
                            tr("Could not add change %1").arg(s.remove.name));
                      else
                        m_controller->refreshMoo();
                    },
                    [this, uid](const AddUnit &s) {
                      m_units->beginAdd();
                      bool success = m_controller->applyAddUnit(s, uid);
                      m_units->endAdd();
                      changeEditState([&](EditState &s) {
                        if (uid == m_controller->uid() && success) {
                          const UnitIdMap &unitIdMap =
                              m_controller->unitIdMap();
                          s.m_current_unit_id =
                              unitIdMap.noToId(unitIdMap.numUnits() - 1);
                        }
                      });
                    },
                    [this, uid](const RemoveUnit &s) {
                      auto current_unit_no = m_controller->unitIdMap().idToNo(
                          m_edit_state.m_current_unit_id);
                      m_units->beginRemove(current_unit_no.value());
                      m_controller->applyRemoveUnit(s, uid);
                      m_units->endRemove();
                      changeEditState([&](EditState &s) {
                        if (current_unit_no != std::nullopt) {
                          const UnitIdMap &unitIdMap =
                              m_controller->unitIdMap();
                          if (unitIdMap.numUnits() <= current_unit_no.value() &&
                              unitIdMap.numUnits() > 0) {
                            s.m_current_unit_id =
                                unitIdMap.noToId(current_unit_no.value() - 1);
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
  // qDebug() << "Changing unit id" << m_client->editState().m_current_unit_id
  //         << m_sync->unitIdMap().noToId(unit_no);
  changeEditState([&](EditState &s) {
    s.m_current_unit_id = m_controller->unitIdMap().noToId(unit_no);
  });
}
