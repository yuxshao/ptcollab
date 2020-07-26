#include "PxtoneSideMenu.h"

#include <QMessageBox>

#include "editor/ComboOptions.h"

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
PxtoneSideMenu::PxtoneSideMenu(PxtoneClient *client, QWidget *parent)
    : SideMenu(new UnitListModel(client, parent),
               new DelayEffectModel(client, parent),
               new OverdriveEffectModel(client, parent), parent),
      m_client(client) {
  setEditWidgetsEnabled(false);
  // TODO: Update this
  connect(m_client, &PxtoneClient::editStateChanged, this,
          &PxtoneSideMenu::handleNewEditState);
  connect(m_client, &PxtoneClient::connected,
          [this]() { setEditWidgetsEnabled(true); });
  connect(m_client->controller(), &PxtoneController::edited,
          [=]() { setModified(true); });
  connect(m_client, &PxtoneClient::userListChanged, this,
          &SideMenu::setUserList);
  connect(m_client->controller(), &PxtoneController::woicesChanged, this,
          &PxtoneSideMenu::refreshWoices);
  connect(m_client->controller(), &PxtoneController::tempoBeatChanged, this,
          &PxtoneSideMenu::refreshTempoBeat);
  connect(m_client, &PxtoneClient::playStateChanged, this, &SideMenu::setPlay);
  connect(this, &SideMenu::currentUnitChanged, m_client,
          &PxtoneClient::setCurrentUnitNo);
  connect(this, &SideMenu::tempoChanged,
          [this](int tempo) { m_client->sendAction(TempoChange{tempo}); });
  connect(this, &SideMenu::beatsChanged,
          [this](int beat) { m_client->sendAction(BeatChange{beat}); });
  connect(this, &SideMenu::addUnit, [this](int woice_id, QString unit_name) {
    m_client->sendAction(AddUnit{
        woice_id, m_client->pxtn()->Woice_Get(woice_id)->get_name_buf(nullptr),
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
        woice->read(&d, a.type);
        m_client->pxtn()->Woice_ReadyTone(woice);
      }
      // TODO: stop existing note preview on creation of new one in this case
      m_note_preview = std::make_unique<NotePreview>(
          m_client->pxtn(), &m_client->moo()->params,
          m_client->editState().mouse_edit_state.last_pitch,
          m_client->editState().mouse_edit_state.base_velocity, 48000, woice,
          this);
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
          m_client->pxtn()->Woice_Get(idx), this);
    else
      m_note_preview = nullptr;
    m_client->setCurrentWoiceNo(idx);
  });
  connect(this, &SideMenu::removeUnit, m_client,
          &PxtoneClient::removeCurrentUnit);

  connect(this, &SideMenu::playButtonPressed, m_client,
          &PxtoneClient::togglePlayState);
  connect(this, &SideMenu::stopButtonPressed, m_client,
          &PxtoneClient::resetAndSuspendAudio);
  connect(this, &SideMenu::paramKindIndexChanged, [this](int index) {
    m_client->changeEditState(
        [&](EditState &s) { s.m_current_param_kind_idx = index; });
  });
  connect(this, &SideMenu::quantXIndexUpdated, [this](int index) {
    m_client->changeEditState(
        [&](EditState &s) { s.m_quantize_clock_idx = index; });
  });
  connect(this, &SideMenu::quantYIndexUpdated, [this](int index) {
    m_client->changeEditState(
        [&](EditState &s) { s.m_quantize_pitch_idx = index; });
  });
  connect(this, &SideMenu::setRepeat, [this]() {
    if (!m_client->editState().mouse_edit_state.selection.has_value()) return;
    Interval selection =
        m_client->editState().mouse_edit_state.selection.value();
    const pxtnMaster *master = m_client->pxtn()->master;
    int clockPerMeasure = master->get_beat_clock() * master->get_beat_num();
    int newRepeatMeas = selection.start / clockPerMeasure;
    selection.start = newRepeatMeas * clockPerMeasure;
    m_client->changeEditState(
        [&](EditState &s) { s.mouse_edit_state.selection.emplace(selection); });
    m_client->sendAction(SetRepeatMeas{newRepeatMeas});
  });

  connect(this, &SideMenu::setLast, [this]() {
    if (!m_client->editState().mouse_edit_state.selection.has_value()) return;
    Interval selection =
        m_client->editState().mouse_edit_state.selection.value();
    const pxtnMaster *master = m_client->pxtn()->master;
    int clockPerMeasure = master->get_beat_clock() * master->get_beat_num();
    int newLastMeas = (selection.end - 1) / clockPerMeasure + 1;
    selection.end = newLastMeas * clockPerMeasure;
    m_client->changeEditState(
        [&](EditState &s) { s.mouse_edit_state.selection.emplace(selection); });
    m_client->sendAction(SetLastMeas{newLastMeas});
  });
  connect(this, &SideMenu::followChanged, [this](bool follow) {
    m_client->changeEditState(
        [&](EditState &s) { s.m_follow_playhead = follow; });
  });
  connect(this, &SideMenu::addOverdrive,
          [this]() { m_client->sendAction(Overdrive::Add{}); });
  connect(this, &SideMenu::removeOverdrive, [this](int ovdrv_no) {
    m_client->sendAction(Overdrive::Remove{ovdrv_no});
  });
}

void PxtoneSideMenu::refreshWoices() {
  QStringList woices;
  for (int i = 0; i < m_client->pxtn()->Woice_Num(); ++i)
    woices.append(m_client->pxtn()->Woice_Get(i)->get_name_buf(nullptr));
  setWoiceList(woices);
}

void PxtoneSideMenu::refreshTempoBeat() {
  setTempo(m_client->pxtn()->master->get_beat_tempo());
  setBeats(m_client->pxtn()->master->get_beat_num());
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
    setFollow(s.m_follow_playhead);
  if (m_last_edit_state.current_param_kind_idx() != s.current_param_kind_idx())
    setParamKindIndex(s.current_param_kind_idx());
  m_last_edit_state = s;
};
