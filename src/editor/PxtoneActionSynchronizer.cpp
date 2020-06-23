#include "PxtoneActionSynchronizer.h"

#include <QDebug>
#include <QDialog>
PxtoneActionSynchronizer::PxtoneActionSynchronizer(int uid, pxtnService *pxtn,
                                                   QObject *parent)
    : QObject(parent),
      m_uid(uid),
      m_pxtn(pxtn),
      m_unit_id_map(pxtn),
      m_local_index(0),
      m_remote_index(0) {}

EditAction PxtoneActionSynchronizer::applyLocalAction(
    const std::list<Action> &action) {
  bool widthChanged = false;
  m_uncommitted.push_back(
      apply_actions_and_get_undo(action, m_pxtn, &widthChanged, m_unit_id_map));
  if (widthChanged) emit measureNumChanged();
  // qDebug() << "Remote" << m_remote_index << "Local" << m_local_index;
  // qDebug() << "New action";
  return EditAction{m_local_index++, action};
}

void PxtoneActionSynchronizer::setUid(qint64 uid) { m_uid = uid; }
qint64 PxtoneActionSynchronizer::uid() { return m_uid; }

void PxtoneActionSynchronizer::applyRemoteAction(const EditAction &action,
                                                 qint64 uid) {
  // TODO: tag these things with names!
  // qDebug() << "Remote" << m_remote_index << "Local" << m_local_index;
  // qDebug() << "Received action" << action.idx << "from user" << uid;
  if (uid == m_uid) {
    if (action.idx != m_remote_index) {
      qWarning() << "Received a remote action with index" << action.idx
                 << "that was not the one I expected" << m_remote_index
                 << ". Discarding. ";
      return;
    }
    ++m_remote_index;
  }
  bool widthChanged = false;
  if (uid == m_uid) {
    // TODO: Respond to unexpected indexes smarter. What would make things
    // consistent?
    if (m_uncommitted.size() == 0) {
      qWarning() << "Received a remote action of mine even when I'm not "
                    "expecting any pending remote responses. Discarding. "
                 << action.idx << m_remote_index;
    } else {
      // The server told us that our local action was applied! Put it in the
      // log, but no need to apply any actions since that was already
      // presumptuously applied.

      m_log.emplace_back(uid, action.idx, m_uncommitted.front());
      m_uncommitted.pop_front();
    }
  } else {
    // Undo each of the uncommitted actions
    // TODO: This is probably where you could be smarter and avoid undoing
    // things that don't intersect in bbox
    for (auto uncommitted = m_uncommitted.rbegin();
         uncommitted != m_uncommitted.rend(); ++uncommitted) {
      // int start_size = uncommitted->size();
      *uncommitted = apply_actions_and_get_undo(*uncommitted, m_pxtn,
                                                &widthChanged, m_unit_id_map);
      // qDebug() << "undoing local size(" << start_size << uncommitted->size()
      //         << ") index(" << i-- << ")";
    }

    // apply the committed action
    std::list<Action> reverse = apply_actions_and_get_undo(
        action.action, m_pxtn, &widthChanged, m_unit_id_map);

    // redo each of the uncommitted actions forwards
    for (std::list<Action> &uncommitted : m_uncommitted) {
      // int start_size = uncommitted.size();
      uncommitted = apply_actions_and_get_undo(uncommitted, m_pxtn,
                                               &widthChanged, m_unit_id_map);
      // qDebug() << "redoing local size(" << start_size << uncommitted.size()
      //         << ") index(" << i++ << ")";
    }

    m_log.emplace_back(uid, action.idx, reverse);
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
  // qDebug() << "m_log size" << m_log.size();

  if (widthChanged) emit measureNumChanged();
}

void PxtoneActionSynchronizer::applyUndoRedo(const UndoRedo &r, qint64 uid) {
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
    *uncommitted = apply_actions_and_get_undo(*uncommitted, m_pxtn,
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
        it->reverse = apply_actions_and_get_undo(it->reverse, m_pxtn,
                                                 &widthChanged, m_unit_id_map);
        temporarily_undone.push_front(&(*it));
      }
      ++it;
    }
    it->reverse = apply_actions_and_get_undo(it->reverse, m_pxtn, &widthChanged,
                                             m_unit_id_map);
    it->state = (it->state == LoggedAction::UNDONE ? LoggedAction::DONE
                                                   : LoggedAction::UNDONE);
    ;
    for (LoggedAction *it : temporarily_undone)
      it->reverse = apply_actions_and_get_undo(it->reverse, m_pxtn,
                                               &widthChanged, m_unit_id_map);
  }

  for (std::list<Action> &uncommitted : m_uncommitted) {
    uncommitted = apply_actions_and_get_undo(uncommitted, m_pxtn, &widthChanged,
                                             m_unit_id_map);
  }

  if (widthChanged) emit measureNumChanged();
}

// TODO: Could probably also make adding and deleting units undoable. The main
// thing is they'd have to be added to the log. And a record of a delete needs
// to include the notes that were deleted with it.
bool PxtoneActionSynchronizer::applyAddUnit(const AddUnit &a, qint64 uid) {
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

  m_unit_id_map.addUnit();
  int unit_no = m_pxtn->Unit_Num() - 1;
  pxtnUnit *unit = m_pxtn->Unit_Get_variable(unit_no);
  std::string unit_name_str =
      a.unit_name.toStdString().substr(0, pxtnMAX_TUNEUNITNAME);
  const char *unit_name_buf = unit_name_str.c_str();
  unit->set_name_buf(unit_name_buf, int32_t(unit_name_str.length()));
  m_pxtn->evels->Record_Add_i(0, unit_no, EVENTKIND_VOICENO, a.woice_id);
  unit->Tone_Init();
  // TODO: This is sort of bad to do, to use a private moo fn for the purposes
  // of note previews. We really need to split out the moo state.
  m_pxtn->moo_ResetVoiceOn_Custom(unit, a.woice_id);
  return true;
}

void PxtoneActionSynchronizer::applyRemoveUnit(const RemoveUnit &a,
                                               qint64 uid) {
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
  } else {
    m_unit_id_map.removeUnit(unit_no);
  }
}

bool PxtoneActionSynchronizer::applyAddWoice(const AddWoice &a, qint64 uid) {
  (void)uid;
  pxtnDescriptor d;
  d.set_memory_r(a.data.constData(), a.data.size());
  pxtnERR result = m_pxtn->Woice_read(m_pxtn->Woice_Num(), &d, a.type);
  if (result != pxtnOK) {
    qDebug() << "Woice_read error" << result;
    return false;
  }
  QString name(a.name);
  name.truncate(pxtnMAX_TUNEWOICENAME);
  int32_t woice_idx = m_pxtn->Woice_Num() - 1;
  m_pxtn->Woice_Get_variable(woice_idx)->set_name_buf(
      name.toStdString().c_str(), name.length());
  m_pxtn->Woice_ReadyTone(woice_idx);
  return true;
}

// TODO: Once you add the ability for units to change instruments,
// you'll need a map for voices.
bool PxtoneActionSynchronizer::applyRemoveWoice(const RemoveWoice &a,
                                                qint64 uid) {
  (void)uid;
  if (m_pxtn->Woice_Num() == 1) {
    qWarning() << "Cannot remove last woice.";
    return false;
  }
  QString expected_name(a.name);
  expected_name.truncate(pxtnMAX_TUNEWOICENAME);

  const pxtnWoice *woice = m_pxtn->Woice_Get(a.id);
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

  if (!m_pxtn->Woice_Remove(a.id)) {
    qWarning() << "Could not remove woice" << a.id << expected_name;
    return false;
  }
  m_pxtn->evels->Record_Value_Omit(EVENTKIND_VOICENO, a.id);
  return true;
}
