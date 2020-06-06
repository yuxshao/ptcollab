#ifndef PXTONEACTIONSYNCHRONIZER_H
#define PXTONEACTIONSYNCHRONIZER_H

#include <list>

#include "protocol/PxtoneEditAction.h"
#include "protocol/RemoteAction.h"

// Okay, I give up on eager undo. It's just way too hard to roll back an undo
// from the local branch.

struct LoggedAction {
  enum UndoState { DONE, UNDONE, GONE };
  UndoState state;
  int uid;
  int idx;
  std::vector<Action>
      reverse;  // TODO: Figure out where to call sth undo vs. reverse
  LoggedAction(int uid, int idx, const std::vector<Action> &reverse)
      : state(DONE), uid(uid), idx(idx), reverse(reverse) {}
};

class PxtoneActionSynchronizer {
 public:
  PxtoneActionSynchronizer(int uid, pxtnEvelist *evels);

  RemoteAction applyLocalAction(const std::vector<Action> &action);
  // Not a great API. One applies but the other just gets actions?
  // And you have to make sure the caller sends them over to the server in the
  // right order.
  RemoteAction getUndo();
  RemoteAction getRedo();

  void applyRemoteAction(int uid, const RemoteAction &action);

 private:
  int m_uid;
  pxtnEvelist *m_evels;
  std::vector<LoggedAction> m_log;
  std::list<std::vector<Action>> m_uncommitted;
  int m_local_index;
  int m_remote_index;
};

#endif  // PXTONEACTIONSYNCHRONIZER_H
