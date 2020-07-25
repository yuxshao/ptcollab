#include "PxtoneController.h"

#include <QDebug>
#include <QDialog>
PxtoneController::PxtoneController(int uid, pxtnService *pxtn,
                                   mooState *moo_state, QObject *parent)
    : QObject(parent),
      m_uid(uid),
      m_pxtn(pxtn),
      m_moo_state(moo_state),
      m_unit_id_map(pxtn),
      m_remote_index(0) {}

EditAction PxtoneController::applyLocalAction(
    const std::list<Action::Primitive> &action) {
  bool widthChanged = false;
  m_uncommitted.push_back(
      Action::apply_and_get_undo(action, m_pxtn, &widthChanged, m_unit_id_map));
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
      *uncommitted = Action::apply_and_get_undo(*uncommitted, m_pxtn,
                                                &widthChanged, m_unit_id_map);
      // qDebug() << "undoing local size(" << start_size << uncommitted->size()
      //         << ") index(" << i-- << ")";
    }

    // apply the committed action
    std::list<Action::Primitive> reverse = Action::apply_and_get_undo(
        action.action, m_pxtn, &widthChanged, m_unit_id_map);

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
      uncommitted = Action::apply_and_get_undo(uncommitted, m_pxtn,
                                               &widthChanged, m_unit_id_map);
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
    *uncommitted = Action::apply_and_get_undo(*uncommitted, m_pxtn,
                                              &widthChanged, m_unit_id_map);
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
        it->reverse = Action::apply_and_get_undo(it->reverse, m_pxtn,
                                                 &widthChanged, m_unit_id_map);
        temporarily_undone.push_front(&(*it));
      }
      ++it;
    }
    it->reverse = Action::apply_and_get_undo(it->reverse, m_pxtn, &widthChanged,
                                             m_unit_id_map);
    it->state = (it->state == LoggedAction::UNDONE ? LoggedAction::DONE
                                                   : LoggedAction::UNDONE);
    ;
    for (LoggedAction *it : temporarily_undone)
      it->reverse = Action::apply_and_get_undo(it->reverse, m_pxtn,
                                               &widthChanged, m_unit_id_map);
  }

  for (std::list<Action::Primitive> &uncommitted : m_uncommitted) {
    uncommitted = Action::apply_and_get_undo(uncommitted, m_pxtn, &widthChanged,
                                             m_unit_id_map);
  }

  if (widthChanged) emit measureNumChanged();
  emit edited();
}

// TODO: Could probably also make adding and deleting units undoable. The main
// thing is they'd have to be added to the log. And a record of a delete needs
// to include the notes that were deleted with it.
bool PxtoneController::applyAddUnit(const AddUnit &a, qint64 uid) {
  (void)uid;
  if (m_pxtn->Woice_Num() <= a.woice_id || a.woice_id < 0) {
    qWarning("Voice doesn't exist. (ID out of bounds)");
    return false;
  }
  QString woice_name = m_pxtn->Woice_Get(a.woice_id)->get_name_buf(nullptr);
  if (woice_name != a.woice_name) {
    qWarning("Voice doesn't exist. (Name doesn't match)");
    return false;
  }
  if (!m_pxtn->Unit_AddNew()) {
    qWarning("Could not add another unit.");
    return false;
  }
  m_moo_state->addUnit(m_pxtn->Woice_Get(a.woice_id));

  m_unit_id_map.addUnit();
  int unit_no = m_pxtn->Unit_Num() - 1;
  pxtnUnit *unit = m_pxtn->Unit_Get_variable(unit_no);
  std::string unit_name_str =
      a.unit_name.toStdString().substr(0, pxtnMAX_TUNEUNITNAME);
  const char *unit_name_buf = unit_name_str.c_str();
  unit->set_name_buf(unit_name_buf, int32_t(unit_name_str.length()));
  m_pxtn->evels->Record_Add_i(0, unit_no, EVENTKIND_VOICENO, a.woice_id);
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
  if (!m_pxtn->Unit_Remove(unit_no)) {
    qWarning("Could not remove unit.");
    return;
  }
  m_unit_id_map.removeUnit(unit_no);
  if (m_moo_state->units.size() > size_t(unit_no))
    m_moo_state->units.erase(m_moo_state->units.begin() + unit_no);

  emit edited();
}

bool PxtoneController::applyTempoChange(const TempoChange &a, qint64 uid) {
  (void)uid;
  if (a.tempo < 20 || a.tempo > 600) return false;
  m_pxtn->adjustTempo(a.tempo, *m_moo_state);
  emit edited();
  return true;
}

bool PxtoneController::applyBeatChange(const BeatChange &a, qint64 uid) {
  (void)uid;
  if (a.beat < 1 || a.beat > 16) return false;
  m_pxtn->adjustBeatNum(a.beat, *m_moo_state);
  emit edited();
  return true;
}

void PxtoneController::applySetRepeatMeas(const SetRepeatMeas &a, qint64 uid) {
  (void)uid;
  m_pxtn->master->set_repeat_meas(a.meas);
  emit edited();
}

void PxtoneController::applySetLastMeas(const SetLastMeas &a, qint64 uid) {
  (void)uid;
  m_pxtn->master->set_last_meas(a.meas);
  emit edited();
}

void PxtoneController::seekMoo(int64_t clock) {
  pxtnVOMITPREPARATION prep{};
  prep.flags |= pxtnVOMITPREPFLAG_loop | pxtnVOMITPREPFLAG_unit_mute;
  prep.start_pos_sample = clock * 60 * 44100 /
                          m_pxtn->master->get_beat_clock() /
                          m_pxtn->master->get_beat_tempo();
  prep.master_volume = 0.80f;
  bool success = m_pxtn->moo_preparation(&prep, *m_moo_state);
  if (!success) qWarning() << "Moo preparation error";
}

void PxtoneController::refreshMoo() {
  seekMoo(m_pxtn->moo_get_now_clock(*m_moo_state));
}

bool PxtoneController::loadDescriptor(pxtnDescriptor &desc) {
  if (desc.get_size_bytes() > 0) {
    if (m_pxtn->read(&desc) != pxtnOK) {
      qWarning() << "Error reading pxtone data from descriptor";
      return false;
    }
  }
  m_unit_id_map = UnitIdMap(m_pxtn);
  if (m_pxtn->tones_ready(*m_moo_state) != pxtnOK) {
    qWarning() << "Error getting tones ready";
    return false;
  }

  emit measureNumChanged();
  return true;
}

bool PxtoneController::applyAddWoice(const AddWoice &a, qint64 uid) {
  (void)uid;
  pxtnDescriptor d;
  d.set_memory_r(a.data.constData(), a.data.size());
  pxtnERR result = m_pxtn->Woice_read(m_pxtn->Woice_Num(), &d, a.type);
  if (result != pxtnOK) {
    qDebug() << "Woice_read error" << result;
    return false;
  }
  int32_t woice_idx = m_pxtn->Woice_Num() - 1;
  std::shared_ptr<pxtnWoice> woice = m_pxtn->Woice_Get_variable(woice_idx);

  QString name(a.name);
  name.truncate(pxtnMAX_TUNEWOICENAME);
  woice->set_name_buf(name.toStdString().c_str(), name.length());
  m_pxtn->Woice_ReadyTone(woice);
  emit edited();
  return true;
}

bool validateRemoveName(const RemoveWoice &a, const pxtnService *pxtn) {
  QString expected_name(a.name);
  expected_name.truncate(pxtnMAX_TUNEWOICENAME);

  std::shared_ptr<const pxtnWoice> woice = pxtn->Woice_Get(a.id);
  if (woice == nullptr) {
    qWarning() << "Received command to remove woice" << a.id
               << "that doesn't exist.";
    return false;
  }

  QString actual_name(woice->get_name_buf(nullptr));
  if (actual_name != expected_name) {
    qWarning() << "Received command to remove woice" << a.id
               << "with mismatched name" << expected_name << actual_name;
    return false;
  }
  return true;
}
// TODO: Once you add the ability for units to change instruments,
// you'll need a map for voices.
bool PxtoneController::applyRemoveWoice(const RemoveWoice &a, qint64 uid) {
  (void)uid;
  if (m_pxtn->Woice_Num() == 1) {
    qWarning() << "Cannot remove last woice.";
    return false;
  }
  if (!validateRemoveName(a, m_pxtn)) return false;

  if (!m_pxtn->Woice_Remove(a.id)) {
    qWarning() << "Could not remove woice" << a.id << a.name;
    return false;
  }
  m_pxtn->evels->Record_Value_Omit(EVENTKIND_VOICENO, a.id);
  emit edited();
  return true;
}

bool PxtoneController::applyChangeWoice(const ChangeWoice &a, qint64 uid) {
  (void)uid;

  if (!validateRemoveName(a.remove, m_pxtn)) return false;

  // TODO: Remove duplication with add woice
  pxtnDescriptor d;
  d.set_memory_r(a.add.data.constData(), a.add.data.size());
  std::shared_ptr<pxtnWoice> woice = m_pxtn->Woice_Get_variable(a.remove.id);
  pxtnERR result = woice->read(&d, a.add.type);
  if (result != pxtnOK) {
    qDebug() << "Woice_read error" << result << a.remove.name;
    return false;
  }

  QString name(a.add.name);
  name.truncate(pxtnMAX_TUNEWOICENAME);
  woice->set_name_buf(name.toStdString().c_str(), name.length());
  m_pxtn->Woice_ReadyTone(woice);
  emit edited();
  return true;
}
