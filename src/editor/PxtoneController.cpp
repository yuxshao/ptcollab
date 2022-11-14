#include "PxtoneController.h"

#include <QDebug>
#include <QDialog>
#include <QTextCodec>

const QTextCodec *shift_jis_codec = QTextCodec::codecForName("Shift-JIS");

PxtoneController::PxtoneController(int uid, pxtnService *pxtn,
                                   mooState *moo_state, QObject *parent)
    : QObject(parent),
      m_uid(uid),
      m_pxtn(pxtn),
      m_moo_state(moo_state),
      m_unit_id_map(pxtn->Unit_Num()),
      m_woice_id_map(pxtn->Woice_Num()),
      m_remote_index(0) {}

EditAction PxtoneController::applyLocalAction(
    const std::list<Action::Primitive> &action) {
  bool widthChanged = false;
  m_uncommitted.push_back(Action::apply_and_get_undo(
      action, m_pxtn, &widthChanged, m_unit_id_map, m_woice_id_map));
  if (widthChanged) emit measureNumChanged();
  // qDebug() << "Remote" << m_remote_index << "Local" << m_local_index;
  // qDebug() << "New action";
  emit edited();
  return EditAction{qint64(m_remote_index + m_uncommitted.size() - 1), action};
}

void PxtoneController::setUid(qint64 uid) { m_uid = uid; }
qint64 PxtoneController::uid() { return m_uid; }

void PxtoneController::applyRemoteAction(const EditAction &action, qint64 uid) {
  // qDebug() << "Remote" << m_remote_index << "Local" << m_local_index;
  // qDebug() << "Received action" << action.idx << "from user" << uid;
  bool widthChanged = false;
  // Try to be liberal in what I accept:
  // If the action index is what I expect, just increment the remote counter
  // (need to undo = false). If the action's index is too high, drop whatever
  // was missing in between (undo true, drop things). If it's too low, apply it
  // like you would someone else's action (undo true, drop 0). In any case,
  // everyone is getting this stream in the same order. Even if it looks messed
  // up to you.
  size_t local_actions_to_drop = 0;
  bool need_to_undo = false;
  if (uid == m_uid) {
    if (action.idx >= m_remote_index)
      local_actions_to_drop = action.idx + 1 - m_remote_index;
    if (action.idx != m_remote_index) {
      qWarning() << "Received a remote action with index" << action.idx
                 << "that was not the one I expected" << m_remote_index;
      need_to_undo = true;
    }
    if (m_uncommitted.size() < local_actions_to_drop) {
      qWarning()
          << "Received a remote action of mine from the future. action.idx("
          << action.idx << "), m_remote_index(" << m_remote_index
          << "), m_uncommitted(" << m_uncommitted.size() << ")";
      need_to_undo = true;
    }
  } else {
    need_to_undo = true;
    local_actions_to_drop = 0;
  }

  // Invalidate any previous undone actions by this user
  for (auto it = m_log.rbegin(); it != m_log.rend(); ++it) {
    if (it->uid == uid) switch (it->state) {
        case LoggedAction::UNDONE:
          it->state = LoggedAction::GONE;
          break;
        case LoggedAction::GONE:
          break;
        case LoggedAction::DONE:
          // break out of the outer for with a goto
          goto exit_loop;
      }
  }
exit_loop:

  if (!need_to_undo) {
    // The server told us that our local action was applied! Put it in the
    // log, but no need to apply any actions since that was already
    // presumptuously applied.
    m_log.emplace_back(uid, action.idx, m_uncommitted.front());
    m_uncommitted.pop_front();
  } else {
    // Undo each of the uncommitted actions
    // TODO: This is probably where you could be smarter and avoid undoing
    // things that don't intersect in bbox
    for (auto uncommitted = m_uncommitted.rbegin();
         uncommitted != m_uncommitted.rend(); ++uncommitted) {
      // int start_size = uncommitted->size();
      *uncommitted = Action::apply_and_get_undo(
          *uncommitted, m_pxtn, &widthChanged, m_unit_id_map, m_woice_id_map);
      // qDebug() << "undoing local size(" << start_size << uncommitted->size()
      //         << ") index(" << i-- << ")";
    }

    // apply the committed action
    std::list<Action::Primitive> reverse = Action::apply_and_get_undo(
        action.action, m_pxtn, &widthChanged, m_unit_id_map, m_woice_id_map);

    if (local_actions_to_drop >= m_uncommitted.size())
      m_uncommitted.clear();
    else {
      auto it_end = m_uncommitted.begin();
      advance(it_end, local_actions_to_drop);
      m_uncommitted.erase(m_uncommitted.begin(), it_end);
    }
    // redo each of the uncommitted actions forwards
    for (std::list<Action::Primitive> &uncommitted : m_uncommitted) {
      // int start_size = uncommitted.size();
      uncommitted = Action::apply_and_get_undo(
          uncommitted, m_pxtn, &widthChanged, m_unit_id_map, m_woice_id_map);
      // qDebug() << "redoing local size(" << start_size << uncommitted.size()
      //         << ") index(" << i++ << ")";
    }

    m_log.emplace_back(uid, action.idx, reverse);
  }

  m_remote_index += int(local_actions_to_drop);

  // qDebug() << "m_log size" << m_log.size();

  if (widthChanged) emit measureNumChanged();
  emit edited();
}

void PxtoneController::applyUndoRedo(const UndoRedo &r, qint64 uid) {
  qDebug() << "Applying undo / redo";
  if (m_log.size() == 0) {
    qDebug() << "No actions in the log. Doing nothing.";
    return;
  }

  auto target = m_log.rend();
  // If we're redoing, we're eventually going to want to find the first
  // undone item by this user.
  switch (r) {
    case REDO:
      for (auto it = m_log.begin(); it != m_log.end(); ++it)
        if (it->state == LoggedAction::UNDONE && it->uid == uid) {
          target = std::reverse_iterator(it + 1);
          break;
        }
      break;
    case UNDO:
      // Likewise, if undoing, find the last done item.
      for (auto it = m_log.rbegin(); it != m_log.rend(); ++it)
        if (it->state == LoggedAction::DONE && it->uid == uid) {
          target = it;
          break;
        }
  };
  if (target == m_log.rend()) {
    qDebug() << "User has no more actions to undo / redo in this direction.";
    return;
  }

  bool widthChanged = false;
  for (auto uncommitted = m_uncommitted.rbegin();
       uncommitted != m_uncommitted.rend(); ++uncommitted) {
    *uncommitted = Action::apply_and_get_undo(
        *uncommitted, m_pxtn, &widthChanged, m_unit_id_map, m_woice_id_map);
  }

  // Go back to the target action by user, temporarily undoing
  // done actions by other users. Flip. Then redo the done actions by
  // other users.
  {
    std::list<LoggedAction *> temporarily_undone;
    auto it = m_log.rbegin();
    while (it != target) {
      if (it->state == LoggedAction::UndoState::DONE) {
        qDebug() << "Temporarily undoing " << it->uid << it->idx;
        it->reverse = Action::apply_and_get_undo(
            it->reverse, m_pxtn, &widthChanged, m_unit_id_map, m_woice_id_map);
        temporarily_undone.push_front(&(*it));
      }
      ++it;
    }
    it->reverse = Action::apply_and_get_undo(it->reverse, m_pxtn, &widthChanged,
                                             m_unit_id_map, m_woice_id_map);
    it->state = (it->state == LoggedAction::UNDONE ? LoggedAction::DONE
                                                   : LoggedAction::UNDONE);
    ;
    for (LoggedAction *it : temporarily_undone)
      it->reverse = Action::apply_and_get_undo(
          it->reverse, m_pxtn, &widthChanged, m_unit_id_map, m_woice_id_map);
  }

  for (std::list<Action::Primitive> &uncommitted : m_uncommitted) {
    uncommitted = Action::apply_and_get_undo(uncommitted, m_pxtn, &widthChanged,
                                             m_unit_id_map, m_woice_id_map);
  }

  if (widthChanged) emit measureNumChanged();
  emit edited();
}

bool auxSetUnitName(pxtnUnit *unit, QString name) {
  QByteArray unit_name_str = shift_jis_codec->fromUnicode(name);
  return unit->set_name_buf_jis(
      unit_name_str.data(),
      std::min(pxtnMAX_TUNEUNITNAME, int32_t(unit_name_str.length())));
}

// TODO: Could probably also make adding and deleting units undoable. The main
// thing is they'd have to be added to the log. And a record of a delete needs
// to include the notes that were deleted with it.
bool PxtoneController::applyAddUnit(const AddUnit &a, qint64 uid) {
  (void)uid;
  if (m_pxtn->Woice_Num() <= a.woice_no || a.woice_no < 0) {
    qWarning("Voice doesn't exist. (ID out of bounds)");
    return false;
  }
  QString woice_name = shift_jis_codec->toUnicode(
      m_pxtn->Woice_Get(a.woice_no)->get_name_buf_jis(nullptr));
  if (woice_name != a.woice_name) {
    qWarning("Voice doesn't exist. (Name doesn't match)");
    return false;
  }
  emit beginAddUnit();
  if (!m_pxtn->Unit_AddNew()) {
    qWarning("Could not add another unit.");
    return false;
  }
  m_moo_state->addUnit(m_pxtn->Woice_Get(a.woice_no));

  m_unit_id_map.add();
  int unit_no = m_pxtn->Unit_Num() - 1;
  auxSetUnitName(m_pxtn->Unit_Get_variable(unit_no), a.unit_name);
  m_pxtn->evels->Record_Add_i(0, unit_no, EVENTKIND_VOICENO, a.woice_no);
  if (a.starting_volume != EVENTDEFAULT_VOLUME)
    m_pxtn->evels->Record_Add_i(0, unit_no, EVENTKIND_VOLUME,
                                a.starting_volume);
  emit endAddUnit();

  emit edited();
  return true;
}

void PxtoneController::applyRemoveUnit(const RemoveUnit &a, qint64 uid) {
  (void)uid;
  auto unit_no_maybe = m_unit_id_map.idToNo(a.unit_id);
  if (unit_no_maybe == std::nullopt) {
    qWarning("Unit ID (%d) doesn't exist.", a.unit_id);
    return;
  }
  qint32 unit_no = unit_no_maybe.value();
  m_pxtn->evels->Record_UnitNo_Miss(unit_no);
  emit beginRemoveUnit(unit_no);
  if (!m_pxtn->Unit_Remove(unit_no)) {
    qWarning("Could not remove unit.");
    return;
  }
  m_unit_id_map.remove(unit_no);
  if (m_moo_state->units.size() > size_t(unit_no))
    m_moo_state->units.erase(m_moo_state->units.begin() + unit_no);
  emit endRemoveUnit();

  emit edited();
}

void PxtoneController::applySetUnitName(const SetUnitName &a, qint64 uid) {
  (void)uid;
  auto unit_no_maybe = m_unit_id_map.idToNo(a.unit_id);
  if (unit_no_maybe == std::nullopt) {
    qWarning("Unit ID (%d) doesn't exist.", a.unit_id);
    return;
  }
  auxSetUnitName(m_pxtn->Unit_Get_variable(unit_no_maybe.value()), a.name);
  emit unitNameEdited(unit_no_maybe.value());
  emit edited();
}

void PxtoneController::applyMoveUnit(const MoveUnit &a, qint64 uid) {
  (void)uid;
  auto unit_no_maybe = m_unit_id_map.idToNo(a.unit_id);
  if (unit_no_maybe == std::nullopt) {
    qWarning("Unit ID (%d) doesn't exist.", a.unit_id);
    return;
  }
  int unit_no = unit_no_maybe.value();
  int new_unit_no = unit_no + (a.up ? -1 : 1);
  if (new_unit_no < 0 || size_t(new_unit_no) >= m_unit_id_map.numUnits()) {
    qWarning("Unit is at end.");
    return;
  }

  emit beginMoveUnit(unit_no, a.up);
  m_pxtn->evels->Record_UnitNo_Replace(unit_no, new_unit_no);
  m_pxtn->Unit_Replace(unit_no, new_unit_no, *m_moo_state);
  m_unit_id_map.swapAdjacent(unit_no, new_unit_no);
  emit endMoveUnit();
  emit edited();
}

bool PxtoneController::applyTempoChange(const TempoChange &a, qint64 uid) {
  (void)uid;
  if (a.tempo < 20 || a.tempo > 600) return false;
  m_pxtn->adjustTempo(a.tempo, *m_moo_state);
  for (int i = 0; i < m_pxtn->Delay_Num(); ++i)
    m_pxtn->Delay_ReadyTone(i, *m_moo_state);
  emit tempoBeatChanged();
  emit edited();
  return true;
}

bool PxtoneController::applyBeatChange(const BeatChange &a, qint64 uid) {
  (void)uid;
  if (a.beat < 1 || a.beat > 16) return false;
  m_pxtn->adjustBeatNum(a.beat, *m_moo_state);
  for (int i = 0; i < m_pxtn->Delay_Num(); ++i)
    m_pxtn->Delay_ReadyTone(i, *m_moo_state);
  emit tempoBeatChanged();
  emit edited();
  return true;
}

void PxtoneController::applySetRepeatMeas(const SetRepeatMeas &a, qint64 uid) {
  (void)uid;
  int m = a.meas.value_or(0);
  if (m >= m_pxtn->master->get_play_meas())
    m_pxtn->master->set_last_meas(m + 1);
  m_pxtn->master->set_repeat_meas(m);
  emit edited();
}

void PxtoneController::applySetLastMeas(const SetLastMeas &a, qint64 uid) {
  (void)uid;
  int m = a.meas.value_or(0);
  if (m != 0 && m <= m_pxtn->master->get_repeat_meas())
    m_pxtn->master->set_repeat_meas(m - 1);
  m_pxtn->master->set_last_meas(a.meas.value_or(0));
  emit edited();
}

void PxtoneController::applyAddOverdrive(const Overdrive::Add &, qint64 uid) {
  (void)uid;
  if (m_pxtn->OverDrive_Num() >= m_pxtn->OverDrive_Max()) return;
  emit beginAddOverdrive();
  if (m_pxtn->OverDrive_Add(50, 2, 0)) emit edited();
  emit endAddOverdrive();
}

void PxtoneController::applySetOverdrive(const Overdrive::Set &a, qint64 uid) {
  (void)uid;

  // cut and amp are checked in OverDrive_Set so not checked here
  if (a.ovdrv_no > m_pxtn->OverDrive_Max() || a.ovdrv_no < 0 ||
      a.group >= m_pxtn->Group_Num() || a.group < 0)
    return;
  if (!m_pxtn->OverDrive_Set(a.ovdrv_no, a.cut, a.amp, a.group)) return;
  emit overdriveChanged(a.ovdrv_no);
  emit edited();
}

void PxtoneController::applyRemoveOverdrive(const Overdrive::Remove &a,
                                            qint64 uid) {
  (void)uid;
  if (m_pxtn->OverDrive_Num() <= a.ovdrv_no) return;
  emit beginRemoveOverdrive(a.ovdrv_no);
  if (m_pxtn->OverDrive_Remove(a.ovdrv_no)) emit edited();
  emit endRemoveOverdrive();
}

void PxtoneController::applySetDelay(const ZDelay::Set &a, qint64 uid) {
  (void)uid;
  if (a.freq > 1000 || a.freq <= 0.001 || a.rate > 100 || a.rate < 0 ||
      a.group >= m_pxtn->Group_Num() || a.group < 0 || a.unit > DELAYUNIT_max ||
      a.unit < 0 || a.delay_no > m_pxtn->Delay_Max() || a.delay_no < 0)
    return;
  if (!m_pxtn->Delay_Set(a.delay_no, a.unit, a.freq, a.rate, a.group)) return;
  m_pxtn->Delay_ReadyTone(a.delay_no, *m_moo_state);
  emit delayChanged(a.delay_no);
  emit edited();
}

void PxtoneController::seekMoo(int64_t clock) {
  pxtnVOMITPREPARATION prep{};
  prep.flags |= pxtnVOMITPREPFLAG_loop | pxtnVOMITPREPFLAG_unit_mute;
  prep.start_pos_sample = clock * 60 * 44100 /
                          m_pxtn->master->get_beat_clock() /
                          m_pxtn->master->get_beat_tempo();
  prep.master_volume = m_moo_state->params.master_vol;
  bool success = m_pxtn->moo_preparation(&prep, *m_moo_state);
  if (!success) qWarning() << "Moo preparation error";

  emit seeked(clock);
}

void PxtoneController::refreshMoo() {
  seekMoo(m_pxtn->moo_get_now_clock(*m_moo_state));
}

void PxtoneController::setVolume(int volume) {
  double v = volume / 100.0;
  double ampl = pow(25, v - 1);
  if (v < 0.1) ampl *= v / 0.1;
  m_moo_state->params.master_vol = ampl;
}

void PxtoneController::setSongTitle(const QString &title) {
  QByteArray str = shift_jis_codec->fromUnicode(title);
  m_pxtn->text->set_name_buf(str.data(), str.length());
}

void PxtoneController::setSongComment(const QString &comment) {
  QByteArray str = shift_jis_codec->fromUnicode(comment);
  m_pxtn->text->set_comment_buf(str.data(), str.length());
}

bool PxtoneController::loadDescriptor(pxtnDescriptor &desc) {
  emit beginRefresh();
  if (desc.get_size_bytes() > 0) {
    if (m_pxtn->read(&desc) != pxtnOK) {
      qWarning() << "Error reading pxtone data from descriptor";
      return false;
    }
  } else {
    qInfo() << "Empty descriptor (new project) load";
    if (!m_pxtn->clear()) {
      qWarning() << "Error clearing pxtone state";
      return false;
    }
  }
  m_unit_id_map = NoIdMap(m_pxtn->Unit_Num());
  m_woice_id_map = NoIdMap(m_pxtn->Woice_Num());
  if (m_pxtn->tones_ready(*m_moo_state) != pxtnOK) {
    qWarning() << "Error getting tones ready";
    return false;
  }

  // Fill delay eagerly so it's simpler to edit
  // You can't add a 'noop' overdrive unfortunately
  while (m_pxtn->Delay_Num() < m_pxtn->Delay_Max())
    m_pxtn->Delay_Add(DELAYUNIT_Beat, 1, 0, 0, *m_moo_state);

  emit endRefresh();
  emit measureNumChanged();
  emit tempoBeatChanged();
  emit newSong();
  return true;
}

bool PxtoneController::applyAddWoice(const AddWoice &a, qint64 uid) {
  (void)uid;
  pxtnDescriptor d;
  d.set_memory_r(a.data.constData(), a.data.size());
  emit beginAddWoice();
  pxtnERR result = m_pxtn->Woice_read(m_pxtn->Woice_Num(), &d, a.type);
  if (result != pxtnOK) {
    qDebug() << "Woice_read error" << result;
    emit endAddWoice();
    return false;
  }
  emit endAddWoice();
  m_woice_id_map.add();
  int32_t woice_idx = m_pxtn->Woice_Num() - 1;
  std::shared_ptr<pxtnWoice> woice = m_pxtn->Woice_Get_variable(woice_idx);

  QByteArray name_str = shift_jis_codec->fromUnicode(a.name);
  woice->set_name_buf_jis(
      name_str.data(),
      std::min(pxtnMAX_TUNEWOICENAME, int32_t(name_str.length())));
  m_pxtn->Woice_ReadyTone(woice);
  emit edited();
  return true;
}

bool PxtoneController::applyRemoveWoice(const RemoveWoice &a, qint64 uid) {
  (void)uid;
  auto woice_no_maybe = m_woice_id_map.idToNo(a.woice_id);
  if (woice_no_maybe == std::nullopt) {
    qWarning("Voice ID (%d) doesn't exist.", a.woice_id);
    return false;
  }
  int woice_no = woice_no_maybe.value();

  if (m_pxtn->Woice_Num() == 1) {
    qWarning() << "Cannot remove last woice.";
    return false;
  }

  emit beginRemoveWoice(woice_no);
  if (!m_pxtn->Woice_Remove(woice_no)) {
    emit endRemoveWoice();
    qWarning() << "Could not remove woice" << a.woice_id;
    return false;
  }
  emit endRemoveWoice();
  m_woice_id_map.remove(woice_no);
  m_pxtn->evels->Record_Value_Omit(EVENTKIND_VOICENO, woice_no);
  emit edited();
  return true;
}

bool PxtoneController::applyChangeWoice(const ChangeWoice &a, qint64 uid) {
  (void)uid;
  auto woice_no_maybe = m_woice_id_map.idToNo(a.remove.woice_id);
  if (woice_no_maybe == std::nullopt) {
    qWarning("Voice ID (%d) doesn't exist.", a.remove.woice_id);
    return false;
  }
  int woice_no = woice_no_maybe.value();

  // TODO: Remove duplication with add woice
  pxtnDescriptor d;
  d.set_memory_r(a.add.data.constData(), a.add.data.size());
  std::shared_ptr<pxtnWoice> woice = m_pxtn->Woice_Get_variable(woice_no);
  pxtnERR result = woice->read(&d, a.add.type);
  if (result != pxtnOK) {
    qDebug() << "Woice_read error" << result;
    return false;
  }

  QByteArray name_str = shift_jis_codec->fromUnicode(a.add.name);
  woice->set_name_buf_jis(
      name_str.data(),
      std::min(pxtnMAX_TUNEWOICENAME, int32_t(name_str.length())));
  m_pxtn->Woice_ReadyTone(woice);
  emit woiceEdited(woice_no);
  emit edited();
  return true;
}

bool PxtoneController::applyWoiceSet(const Woice::Set &a, qint64 uid) {
  (void)uid;
  std::shared_ptr<pxtnWoice> woice = m_pxtn->Woice_Get_variable(a.id);
  if (woice == nullptr) return false;
  for (int i = 0; i < woice->get_voice_num(); ++i) {
    pxtnVOICEUNIT *voice = woice->get_voice_variable(i);
    std::visit(overloaded{[&](Woice::Key a) {
                            if (a.key > 108 * PITCH_PER_KEY)
                              a.key = 108 * PITCH_PER_KEY;
                            if (a.key < 21 * PITCH_PER_KEY)
                              a.key = 21 * PITCH_PER_KEY;
                            voice->basic_key = a.key;
                          },
                          [&](const Woice::Flag &a) {
                            uint32_t flag;
                            if (woice->get_type() != pxtnWOICE_PTV) {
                              switch (a.flag) {
                                case Woice::Flag::LOOP:
                                  flag = PTV_VOICEFLAG_WAVELOOP;
                                  break;
                                case Woice::Flag::BEATFIT:
                                default:
                                  flag = PTV_VOICEFLAG_BEATFIT;
                                  break;
                              }
                              if (a.set)
                                voice->voice_flags |= flag;
                              else
                                voice->voice_flags &= ~flag;
                            }
                          },
                          [&](const Woice::Name &a) {
                            QByteArray name =
                                shift_jis_codec->fromUnicode(a.name);
                            woice->set_name_buf_jis(
                                name.data(),
                                std::min(pxtnMAX_TUNEWOICENAME, name.length()));
                          }},
               a.setting);
  }
  emit woiceEdited(a.id);
  emit edited();
  return true;
}

void PxtoneController::setUnitPlayed(int unit_no, bool played) {
  pxtnUnit *u = m_pxtn->Unit_Get_variable(unit_no);
  if (!u) return;
  u->set_played(played);
  emit playedToggled(unit_no);
}
void PxtoneController::setUnitVisible(int unit_no, bool visible) {
  pxtnUnit *u = m_pxtn->Unit_Get_variable(unit_no);
  if (!u) return;
  u->set_visible(visible);
}
void PxtoneController::setUnitOperated(int unit_no, bool operated) {
  pxtnUnit *u = m_pxtn->Unit_Get_variable(unit_no);
  if (!u) return;
  u->set_operated(operated);
  emit operatedToggled(unit_no, operated);
}

// If currently this unit is soloing, unmute other selected units. If exactly
// selected units are playing, unmute everything. Else mute everything but
// this unit.
void PxtoneController::cycleSolo(int solo_unit_no) {
  pxtnUnit *solo_u = m_pxtn->Unit_Get_variable(solo_unit_no);
  if (!solo_u) return;

  bool is_currently_soloing = true;
  bool is_currently_selected_soloing = true;
  for (int i = 0; i < m_pxtn->Unit_Num(); ++i) {
    pxtnUnit *u = m_pxtn->Unit_Get_variable(i);
    if (u->get_played() != (u == solo_u)) is_currently_soloing = false;
    if (u->get_played() != (u->get_operated() || u == solo_u))
      is_currently_selected_soloing = false;
  }

  if (is_currently_soloing && !is_currently_selected_soloing) {
    for (int i = 0; i < m_pxtn->Unit_Num(); ++i) {
      pxtnUnit *u = m_pxtn->Unit_Get_variable(i);
      u->set_played(u->get_operated() || u == solo_u);
    }
  } else if (is_currently_selected_soloing) {
    for (int i = 0; i < m_pxtn->Unit_Num(); ++i)
      m_pxtn->Unit_Get_variable(i)->set_played(true);
  } else {
    for (int i = 0; i < m_pxtn->Unit_Num(); ++i) {
      pxtnUnit *u = m_pxtn->Unit_Get_variable(i);
      m_pxtn->Unit_Get_variable(i)->set_played(u == solo_u);
    }
  }
  emit soloToggled();
}

// from
// https://stackoverflow.com/questions/40238343/qfile-write-a-wav-header-writes-only-4-byte-data
struct WavHdr {
  constexpr static quint32 k_riff_id = 0x46464952;
  constexpr static quint32 k_wave_format = 0x45564157;
  constexpr static quint32 k_fmt_id = 0x20746d66;
  constexpr static quint32 k_data_id = 0x61746164;
  // RIFF
  quint32 chunk_id = k_riff_id;
  quint32 chunk_size;
  quint32 chunk_format = k_wave_format;
  // fmt
  quint32 fmt_id = k_fmt_id;
  quint32 fmt_size;
  quint16 audio_format;
  quint16 num_channels;
  quint32 sample_rate;
  quint32 byte_rate;
  quint16 block_align;
  quint16 bits_per_sample;
  // data
  quint32 data_id = k_data_id;
  quint32 data_size;
};

bool write(QIODevice *dev, const WavHdr &h) {
  QDataStream s{dev};
  s.setByteOrder(QDataStream::LittleEndian);  // for RIFF
  s << h.chunk_id << h.chunk_size << h.chunk_format;
  s << h.fmt_id << h.fmt_size << h.audio_format << h.num_channels
    << h.sample_rate << h.byte_rate << h.block_align << h.bits_per_sample;
  s << h.data_id << h.data_size;
  return s.status() == QDataStream::Ok;
}

// TODO: This kind of file-writing is duplicated a bunch.
bool PxtoneController::render_exn(
    QIODevice *dev, double secs, double fadeout, double volume,
    std::optional<size_t> solo_unit,
    std::function<bool(double progress)> should_continue) const {
  qDebug() << "Rendering" << secs << fadeout;
  WavHdr h;
  h.fmt_size = 16;
  h.audio_format = 1;
  int num_channels, sample_rate;
  m_pxtn->get_destination_quality(&num_channels, &sample_rate);
  h.num_channels = num_channels;
  h.sample_rate = sample_rate;
  h.bits_per_sample = 16;
  h.block_align = h.num_channels * h.bits_per_sample / 8;
  h.byte_rate = h.sample_rate * h.num_channels * h.bits_per_sample / 8;

  int num_samples = int(h.sample_rate * secs);
  if (fadeout > 0) num_samples += int(h.sample_rate * fadeout) + 10;
  h.data_size = num_samples * h.num_channels * h.bits_per_sample / 8;
  h.chunk_size = h.data_size + 36;

  mooState moo_state;
  pxtnERR err;
  err = m_pxtn->tones_ready(moo_state);
  if (err != pxtnOK)
    throw QString("Error getting tones ready: error code %1").arg(err);

  pxtnVOMITPREPARATION prep{};
  prep.flags |= pxtnVOMITPREPFLAG_loop | pxtnVOMITPREPFLAG_unit_mute;
  prep.start_pos_sample = 0;
  prep.master_volume = volume;
  prep.solo_unit = solo_unit;
  bool success = m_pxtn->moo_preparation(&prep, moo_state);
  if (!success) throw QString("Error preparing moo");

  write(dev, h);
  int written = 0;
  auto render = [&](int len) {
    constexpr int SIZE = 4096;
    char buf[SIZE];
    while (written < len) {
      int mooed_len;
      if (!m_pxtn->Moo(moo_state, buf, int32_t(std::min(len - written, SIZE)),
                       &mooed_len))
        throw QString("Moo error during rendering. Bytes written so far: %1")
            .arg(written);
      qint64 written_this_time = dev->write(buf, mooed_len);
      if (written_this_time < mooed_len) {
        throw QString(
            "Could not write all bytes into device this cycle (wrote %1 / %2). "
            "Total bytes written so far: ")
            .arg(written_this_time)
            .arg(mooed_len)
            .arg(written);
      }
      if (!should_continue((0.0 + written) / h.data_size)) return false;

      written += mooed_len;
    }
    return true;
  };

  if (!render(int(h.sample_rate * secs) * h.num_channels * h.bits_per_sample /
              8))
    return false;
  m_pxtn->moo_set_fade(-1, fadeout, moo_state);
  if (!render(h.data_size)) return false;

  return true;
}
