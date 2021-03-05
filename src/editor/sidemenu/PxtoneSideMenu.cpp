#include "PxtoneSideMenu.h"

#include <QMessageBox>

#include "BasicWoiceListModel.h"
#include "editor/ComboOptions.h"
#include "editor/Settings.h"

// TODO: Put this somewhere else, like in remote action or sth.
AddWoice make_addWoice_from_path(const QString &path) {
  QFileInfo fileinfo(path);
  QString filename = fileinfo.fileName();
  QString suffix = fileinfo.suffix().toLower();
  pxtnWOICETYPE type;

  if (suffix == "ptvoice")
    type = pxtnWOICE_PTV;
  else if (suffix == "ptnoise")
    type = pxtnWOICE_PTN;
  else if (suffix == "ogg" || suffix == "oga")
    type = pxtnWOICE_OGGV;
  else if (suffix == "wav")
    type = pxtnWOICE_PCM;
  else {
    throw QString("Voice file (%1) has invalid extension (%2)")
        .arg(filename)
        .arg(suffix);
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    throw QString("Could not open file (%1)").arg(filename);

  QString name = fileinfo.baseName();
  return AddWoice{type, name, file.readAll()};
}

// The model parents are the menu's parents because otherwise there's an init
// cycle.
PxtoneSideMenu::PxtoneSideMenu(PxtoneClient *client, MooClock *moo_clock,
                               QWidget *parent)
    : SideMenu(new UnitListModel(client, parent),
               new WoiceListModel(client, parent),
               new UserListModel(client, parent),
               new SelectWoiceDialog(new BasicWoiceListModel(client, parent),
                                     client, parent),
               new DelayEffectModel(client, parent),
               new OverdriveEffectModel(client, parent)),
      m_client(client),
      m_moo_clock(moo_clock) {
  setEditWidgetsEnabled(false);
  // TODO: Update this
  connect(m_client, &PxtoneClient::editStateChanged, this,
          &PxtoneSideMenu::handleNewEditState);
  connect(m_client, &PxtoneClient::connected,
          [this]() { setEditWidgetsEnabled(true); });
  connect(m_client->controller(), &PxtoneController::tempoBeatChanged, this,
          &PxtoneSideMenu::refreshTempoBeat);
  connect(m_client, &PxtoneClient::playStateChanged, this, &SideMenu::setPlay);
  connect(this, &SideMenu::currentUnitChanged,
          [this](int unit_no) { m_client->setCurrentUnitNo(unit_no, false); });
  connect(this, &SideMenu::tempoChanged,
          [this](int tempo) { m_client->sendAction(TempoChange{tempo}); });
  connect(this, &SideMenu::beatsChanged,
          [this](int beat) { m_client->sendAction(BeatChange{beat}); });
  connect(this, &SideMenu::addUnit, [this](int woice_id, QString unit_name) {
    m_client->sendAction(AddUnit{
        woice_id,
        shift_jis_codec->toUnicode(
            m_client->pxtn()->Woice_Get(woice_id)->get_name_buf_jis(nullptr)),
        unit_name});
  });
  connect(this, &SideMenu::addWoice, [this](QString path) {
    try {
      m_client->sendAction(make_addWoice_from_path(path));
    } catch (const QString &e) {
      QMessageBox::critical(this, tr("Unable to add voice"), e);
    }
  });
  connect(this, &SideMenu::changeWoice,
          [this](int idx, QString name, QString path) {
            try {
              m_client->sendAction(ChangeWoice{RemoveWoice{idx, name},
                                               make_addWoice_from_path(path)});
            } catch (const QString &e) {
              QMessageBox::critical(this, tr("Unable to change voice"), e);
            }
          });

  connect(this, &SideMenu::removeWoice, [this](int idx, QString name) {
    if (m_client->pxtn()->Woice_Num() == 1) {
      QMessageBox::critical(this, tr("Error"), tr("Cannot remove last voice."));
      return;
    }
    if (idx >= 0) m_client->sendAction(RemoveWoice{idx, name});
  });
  connect(this, &SideMenu::candidateWoiceSelected, [this](QString path) {
    try {
      AddWoice a(make_addWoice_from_path(path));
      std::shared_ptr<pxtnWoice> woice = std::make_shared<pxtnWoice>();
      {
        pxtnDescriptor d;
        d.set_memory_r(a.data.constData(), a.data.size());
        pxtnERR result = woice->read(&d, a.type);
        if (result != pxtnOK) throw QString("Invalid voice data");
        m_client->pxtn()->Woice_ReadyTone(woice);
      }
      // TODO: stop existing note preview on creation of new one in this case
      m_note_preview = std::make_unique<NotePreview>(
          m_client->pxtn(), &m_client->moo()->params,
          m_client->editState().mouse_edit_state.last_pitch,
          m_client->editState().mouse_edit_state.base_velocity, 48000, woice,
          m_client->audioState()->bufferSize(), this);
    } catch (const QString &e) {
      qDebug() << "Could not preview woice at path" << path << ". Error" << e;
    }
  });
  connect(this, &SideMenu::selectWoice, [this](int idx) {
    // TODO: Adjust the length based off pitch and if the instrument loops or
    // not. Also this is variable on tempo rn - fix that.
    if (idx >= 0)
      m_note_preview = std::make_unique<NotePreview>(
          m_client->pxtn(), &m_client->moo()->params,
          m_client->editState().mouse_edit_state.last_pitch,
          m_client->editState().mouse_edit_state.base_velocity, 48000,
          m_client->pxtn()->Woice_Get(idx),
          m_client->audioState()->bufferSize(), this);
    else
      m_note_preview = nullptr;
    m_client->setCurrentWoiceNo(idx, false);
  });
  connect(this, &SideMenu::unitClicked, [this](int idx) {
    if (!Settings::UnitPreviewClick::get()) return;
    if (idx >= 0)
      m_note_preview = std::make_unique<NotePreview>(
          m_client->pxtn(), &m_client->moo()->params, idx, m_moo_clock->now(),
          48000, std::list<EVERECORD>(), m_client->audioState()->bufferSize(),
          false, this);
    else
      m_note_preview = nullptr;
    m_client->setCurrentWoiceNo(idx, false);
  });
  connect(this, &SideMenu::removeUnit, m_client,
          &PxtoneClient::removeCurrentUnit);

  connect(this, &SideMenu::playButtonPressed, m_client,
          &PxtoneClient::togglePlayState);
  connect(this, &SideMenu::stopButtonPressed, m_client,
          &PxtoneClient::resetAndSuspendAudio);
  connect(this, &SideMenu::paramKindIndexActivated, [this](int index) {
    m_client->changeEditState(
        [&](EditState &s) { s.m_current_param_kind_idx = index; }, false);
  });
  connect(this, &SideMenu::quantXIndexActivated, [this](int index) {
    m_client->changeEditState(
        [&](EditState &s) { s.m_quantize_clock_idx = index; }, false);
  });
  connect(this, &SideMenu::quantYIndexActivated, [this](int index) {
    m_client->changeEditState(
        [&](EditState &s) { s.m_quantize_pitch_idx = index; }, false);
  });

  connect(this, &SideMenu::followPlayheadClicked,
          [this](FollowPlayhead follow) {
            m_client->changeEditState(
                [&](EditState &s) { s.m_follow_playhead = follow; }, false);
          });
  connect(this, &SideMenu::copyChanged, [this](bool copy) {
    EVENTKIND kind =
        paramOptions[m_client->editState().current_param_kind_idx()].second;
    if (m_client->clipboard()->kindIsCopied(kind) != copy)
      m_client->clipboard()->setKindIsCopied(kind, copy);
  });
  connect(m_client->clipboard(), &Clipboard::copyKindsSet, this,
          &PxtoneSideMenu::refreshCopyCheckbox);
  connect(this, &SideMenu::addOverdrive,
          [this]() { m_client->sendAction(Overdrive::Add{}); });
  connect(this, &SideMenu::removeOverdrive, [this](int ovdrv_no) {
    m_client->sendAction(Overdrive::Remove{ovdrv_no});
  });
  connect(this, &SideMenu::moveUnit, [this](bool up) {
    m_client->sendAction(MoveUnit{m_client->editState().m_current_unit_id, up});
  });
  connect(this, &SideMenu::volumeChanged, m_client, &PxtoneClient::setVolume);
  connect(this, &SideMenu::bufferLengthChanged, m_client,
          &PxtoneClient::setBufferSize);
  connect(this, &SideMenu::userSelected, [this](int user_no) {
    if (m_client->remoteEditStates().size() <= uint(user_no)) return;
    auto it = m_client->remoteEditStates().begin();
    std::advance(it, user_no);
    m_client->setFollowing(it->first);
  });
  refreshCopyCheckbox();
}

void PxtoneSideMenu::refreshTempoBeat() {
  setTempo(m_client->pxtn()->master->get_beat_tempo());
  setBeats(m_client->pxtn()->master->get_beat_num());
}

void PxtoneSideMenu::refreshCopyCheckbox() {
  EVENTKIND kind =
      paramOptions[m_client->editState().current_param_kind_idx()].second;
  setCopy(m_client->clipboard()->kindIsCopied(kind));
}

void PxtoneSideMenu::handleNewEditState(const EditState &s) {
  if (m_last_edit_state.m_quantize_clock_idx != s.m_quantize_clock_idx)
    setQuantXIndex(s.m_quantize_clock_idx);
  if (m_last_edit_state.m_quantize_pitch_idx != s.m_quantize_pitch_idx)
    setQuantYIndex(s.m_quantize_pitch_idx);
  if (m_last_edit_state.m_current_unit_id != s.m_current_unit_id) {
    std::optional<qint32> x = m_client->unitIdMap().idToNo(s.m_current_unit_id);
    if (x.has_value()) setCurrentUnit(x.value());
  }
  if (m_last_edit_state.m_current_woice_id != s.m_current_woice_id) {
    std::optional<qint32> x =
        m_client->controller()->woiceIdMap().idToNo(s.m_current_woice_id);
    if (x.has_value()) setCurrentWoice(x.value());
  }
  if (m_last_edit_state.m_follow_playhead != s.m_follow_playhead)
    setFollowPlayhead(s.m_follow_playhead);
  if (m_last_edit_state.current_param_kind_idx() !=
      s.current_param_kind_idx()) {
    setParamKindIndex(s.current_param_kind_idx());
    refreshCopyCheckbox();
  }

  m_last_edit_state = s;
};
