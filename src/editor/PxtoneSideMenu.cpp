#include "PxtoneSideMenu.h"

#include <QMessageBox>

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

PxtoneSideMenu::PxtoneSideMenu(PxtoneClient *client, UnitListModel *units,
                               QWidget *parent)
    : SideMenu(units, parent), m_client(client) {
  setEditWidgetsEnabled(false);
  // TODO: Update this
  connect(m_client, &PxtoneClient::editStateChanged, this,
          &PxtoneSideMenu::handleNewEditState);
  connect(m_client, &PxtoneClient::connected,
          [this]() { setEditWidgetsEnabled(true); });
  connect(m_client, &PxtoneClient::edited, [=]() { setModified(true); });
  connect(m_client, &PxtoneClient::userListChanged, this,
          &SideMenu::setUserList);
  connect(m_client, &PxtoneClient::woicesChanged, this,
          &PxtoneSideMenu::refreshWoices);
  connect(m_client, &PxtoneClient::tempoBeatChanged, this,
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
  });
  connect(this, &SideMenu::removeUnit, m_client,
          &PxtoneClient::removeCurrentUnit);

  connect(this, &SideMenu::playButtonPressed, m_client,
          &PxtoneClient::togglePlayState);
  connect(this, &SideMenu::stopButtonPressed, m_client,
          &PxtoneClient::resetAndSuspendAudio);
}

void PxtoneSideMenu::refreshWoices() {
  QStringList woices;
  for (int i = 0; i < m_client->pxtn()->Woice_Num(); ++i)
    woices.append(m_client->pxtn()->Woice_Get(i)->get_name_buf(nullptr));
  setWoiceList(QStringList(woices));
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
  if (m_last_edit_state.m_current_unit_id != s.m_current_unit_id)
    setCurrentUnit(s.m_current_unit_id);
  m_last_edit_state = s;
};
