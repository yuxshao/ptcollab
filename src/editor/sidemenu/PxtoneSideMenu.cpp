#include "PxtoneSideMenu.h"

#include <QMessageBox>

#include "BasicWoiceListModel.h"
#include "SongTitleDialog.h"
#include "editor/ComboOptions.h"
#include "editor/Settings.h"

// The model parents are the menu's parents because otherwise there's an init
// cycle.
PxtoneSideMenu::PxtoneSideMenu(PxtoneClient *client, MooClock *moo_clock,
                               NewWoiceDialog *new_woice_dialog,
                               NewWoiceDialog *change_woice_dialog,
                               QWidget *parent)
    : SideMenu(new UnitListModel(client, parent),
               new WoiceListModel(client, parent),
               new UserListModel(client, parent),
               new SelectWoiceDialog(new BasicWoiceListModel(client, parent),
                                     client, parent),
               new DelayEffectModel(client, parent),
               new OverdriveEffectModel(client, parent), new_woice_dialog,
               change_woice_dialog,
               new VolumeMeterFrame(
                   client,
                   nullptr)),  // VolumeMeterWidget gets reparented which causes
                               // some weird lifetime issues with its children
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
  /*connect(m_client, &PxtoneClient::volumeLevelChanged, this,
          [this](const std::vector<VolumeMeter> &levels) {
            qDebug() << QString("Volume: (%1dbfs,%2dbfs), peak (%3dbfs,%4dbfs)")
                            .arg(levels[0].current_volume_dbfs())
                            .arg(levels[1].current_volume_dbfs())
                            .arg(levels[0].last_peak_dbfs())
                            .arg(levels[1].last_peak_dbfs());
            setVolumeMeterLevel((levels[0].current_volume_dbfs() +
                                 levels[1].current_volume_dbfs()) /
                                2);
          });*/

  connect(this, &SideMenu::currentUnitChanged,
          [this](int unit_no) { m_client->setCurrentUnitNo(unit_no, false); });
  connect(this, &SideMenu::tempoChanged,
          [this](int tempo) { m_client->sendAction(TempoChange{tempo}); });
  connect(this, &SideMenu::beatsChanged,
          [this](int beat) { m_client->sendAction(BeatChange{beat}); });
  connect(this, &SideMenu::addUnit, [this](int woice_no, QString unit_name) {
    m_client->sendAction(AddUnit{
        woice_no, Settings::NewUnitDefaultVolume::get(),
        shift_jis_codec->toUnicode(
            m_client->pxtn()->Woice_Get(woice_no)->get_name_buf_jis(nullptr)),
        unit_name});
  });
  connect(this, &SideMenu::addWoices, [this](const std::vector<AddWoice> &ws) {
    uint woice_capacity =
        m_client->pxtn()->Woice_Max() - m_client->pxtn()->Woice_Num();
    if (ws.size() >= woice_capacity)
      QMessageBox::warning(this, tr("Voice add error"),
                           tr("You are trying to add too many voices (%1). "
                              "There is space for %2.")
                               .arg(ws.size())
                               .arg(woice_capacity));
    else
      try {
        for (const AddWoice &w : ws) m_client->sendAction(w);
      } catch (const QString &e) {
        QMessageBox::critical(this, tr("Unable to add voice"), e);
      }
  });
  connect(this, &SideMenu::changeWoice, [this](int idx, const AddWoice &w) {
    try {
      int woice_id = m_client->controller()->woiceIdMap().noToId(idx);
      m_client->sendAction(ChangeWoice{RemoveWoice{woice_id}, w});
    } catch (const QString &e) {
      QMessageBox::critical(this, tr("Unable to change voice"), e);
    }
  });

  connect(this, &SideMenu::removeWoice, [this](int idx) {
    if (m_client->pxtn()->Woice_Num() == 1) {
      QMessageBox::critical(this, tr("Error"), tr("Cannot remove last voice."));
      return;
    }
    if (idx >= 0) {
      int woice_id = m_client->controller()->woiceIdMap().noToId(idx);
      m_client->sendAction(RemoveWoice{woice_id});
    }
  });

  connect(this, &SideMenu::selectWoice, [this](int idx) {
    // TODO: Adjust the length based off pitch and if the instrument loops or
    // not. Also this is variable on tempo rn - fix that.
    if (!m_client->isPlaying()) {
      if (idx >= 0)
        m_note_preview = std::make_unique<NotePreview>(
            m_client->pxtn(), &m_client->moo()->params,
            m_client->editState().mouse_edit_state.last_pitch,
            m_client->editState().mouse_edit_state.base_velocity, 48000,
            m_client->pxtn()->Woice_Get(idx),
            m_client->audioState()->bufferSize(), this);
      else
        m_note_preview = nullptr;
    }
    m_client->setCurrentWoiceNo(idx, false);
  });
  connect(this, &SideMenu::unitClicked, [this](int idx) {
    if (!Settings::UnitPreviewClick::get()) return;
    if (!m_client->isPlaying()) {
      if (idx >= 0)
        m_note_preview = std::make_unique<NotePreview>(
            m_client->pxtn(), &m_client->moo()->params, idx, m_moo_clock->now(),
            48000, std::list<EVERECORD>(), m_client->audioState()->bufferSize(),
            false, this);
      else
        m_note_preview = nullptr;
    }
  });
  connect(this, &SideMenu::removeUnit, m_client,
          &PxtoneClient::removeCurrentUnit);
  connect(this, &SideMenu::selectedUnitsChanged,
          [this](auto &sel, const QItemSelection &des) {
            std::map<int, bool> changes_by_row;
            for (const auto &i : sel.indexes()) changes_by_row[i.row()] = true;
            for (const auto &i : des.indexes()) changes_by_row[i.row()] = false;
            for (const auto &[row, change] : changes_by_row)
              m_client->setUnitOperated(row, change);
          });
  connect(m_client->controller(), &PxtoneController::operatedToggled, this,
          &SideMenu::setUnitSelected);

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
  connect(this, &SideMenu::quantYDenomActivated, [this](int denom) {
    m_client->changeEditState(
        [&](EditState &s) { s.m_quantize_pitch_denom = denom; }, false);
  });

  connect(this, &SideMenu::followPlayheadClicked,
          [this](FollowPlayhead follow) {
            m_client->changeEditState(
                [&](EditState &s) { s.m_follow_playhead = follow; }, false);
          });
  connect(this, &SideMenu::copyChanged, [this](bool copy) {
    EVENTKIND kind =
        paramOptions()[m_client->editState().current_param_kind_idx()].second;
    if (m_client->clipboard()->kindIsCopied(kind) != copy)
      m_client->clipboard()->setKindIsCopied(kind, copy);
  });
  connect(m_client->clipboard(), &Clipboard::copyKindsSet, this,
          &PxtoneSideMenu::refreshCopyCheckbox);
  connect(this, &SideMenu::addOverdrive,
          [this]() { m_client->sendAction(OverdriveEffect::Add{}); });
  connect(this, &SideMenu::removeOverdrive, [this](int ovdrv_no) {
    m_client->sendAction(OverdriveEffect::Remove{ovdrv_no});
  });
  connect(this, &SideMenu::moveUnit, [this](bool up) {
    m_client->sendAction(MoveUnit{m_client->editState().m_current_unit_id, up});
  });
  connect(this, &SideMenu::volumeChanged, m_client, &PxtoneClient::setVolume);
  connect(this, &SideMenu::bufferLengthChanged, m_client,
          &PxtoneClient::setBufferSize);
  connect(this, &SideMenu::userFollowClicked, [this](int user_no) {
    if (m_client->remoteEditStates().size() <= uint(user_no)) return;
    auto it = m_client->remoteEditStates().begin();
    std::advance(it, user_no);
    m_client->setFollowing(it->first);
  });
  connect(this, &SideMenu::userSelected, [this](int user_no) {
    if (m_client->remoteEditStates().size() <= uint(user_no)) return;
    auto it = m_client->remoteEditStates().begin();
    std::advance(it, user_no);
    m_client->jumpToUser(it->first);
  });
  connect(this, &SideMenu::titleCommentBtnClicked, this, [this]() {
    QString title = shift_jis_codec->toUnicode(
        m_client->pxtn()->text->get_name_buf(nullptr));
    QString comment = shift_jis_codec->toUnicode(
        m_client->pxtn()->text->get_comment_buf(nullptr));
    // qDebug() << title << comment;
    SongTitleDialog dialog(title, comment, this);
    if (dialog.exec()) {
      if (title != dialog.title())
        m_client->sendAction(SetSongText{SetSongText::Title, dialog.title()});
      if (comment != dialog.comment())
        m_client->sendAction(
            SetSongText{SetSongText::Comment, dialog.comment()});
    }
  });
  refreshCopyCheckbox();
}

void PxtoneSideMenu::refreshTempoBeat() {
  setTempo(m_client->pxtn()->master->get_beat_tempo());
  setBeats(m_client->pxtn()->master->get_beat_num());
}

void PxtoneSideMenu::refreshCopyCheckbox() {
  EVENTKIND kind =
      paramOptions()[m_client->editState().current_param_kind_idx()].second;
  setCopy(m_client->clipboard()->kindIsCopied(kind));
}

void PxtoneSideMenu::handleNewEditState(const EditState &s) {
  if (m_last_edit_state.m_quantize_clock_idx != s.m_quantize_clock_idx)
    setQuantXIndex(s.m_quantize_clock_idx);
  if (m_last_edit_state.m_quantize_pitch_denom != s.m_quantize_pitch_denom)
    setQuantYDenom(s.m_quantize_pitch_denom);

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
