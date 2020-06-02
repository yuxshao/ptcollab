#include "PxtoneActionSynchronizer.h"

#include <QDebug>
PxtoneActionSynchronizer::PxtoneActionSynchronizer(int uid, pxtnEvelist *evels)
    : m_uid(uid), m_evels(evels), m_local_index(0), m_remote_index(0) {}

int PxtoneActionSynchronizer::applyLocalActionAndGetIdx(
    const std::vector<Action> &action) {
  m_uncommitted.push_back(apply_actions_and_get_undo(action, m_evels));
  qDebug() << "Remote" << m_remote_index << "Local" << m_local_index;
  qDebug() << "New action";
  return m_local_index++;
}

void PxtoneActionSynchronizer::applyRemoteAction(
    int uid, int idx, const std::vector<Action> &action) {
  qDebug() << "Remote" << m_remote_index << "Local" << m_local_index;
  qDebug() << "Received action" << idx << "from user" << uid;
  if (uid == m_uid) {
    // TODO: Respond to this smarter
    if (m_uncommitted.size() == 0) {
      qWarning() << "Received a remote action of mine that I did not apply "
                    "locally. Discarding. "
                 << idx << m_remote_index;
    } else if (idx != m_remote_index) {
      qWarning()
          << "Received a remote action with wrong index that was not the one I "
             "expected. Discarding. "
          << idx << m_remote_index;
    } else {
      // The server told us that our local action was applied! Acknowledge it
      // and do nothing.
      m_uncommitted.pop_front();
      ++m_remote_index;
    }
  } else {
    // Undo each of the uncommitted actions
    // TODO: This is probably where you could be smarter and avoid undoing
    // things that don't intersect in bbox
    for (auto uncommitted = m_uncommitted.rbegin();
         uncommitted != m_uncommitted.rend(); ++uncommitted) {
      *uncommitted = apply_actions_and_get_undo(*uncommitted, m_evels);
    }

    // apply the committed action
    apply_actions_and_get_undo(action, m_evels);

    // redo each of the uncommitted actions forwards
    for (std::vector<Action> &uncommitted : m_uncommitted) {
      uncommitted = apply_actions_and_get_undo(uncommitted, m_evels);
    }
  }
}
