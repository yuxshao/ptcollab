#ifndef PXTONEACTIONSYNCHRONIZER_H
#define PXTONEACTIONSYNCHRONIZER_H

#include <list>

#include "PxtoneEditAction.h"

class PxtoneActionSynchronizer {
 public:
  PxtoneActionSynchronizer(int uid, pxtnEvelist *evels);

  int applyLocalActionAndGetIdx(const std::vector<Action> &action);

  void applyRemoteAction(int uid, int idx, const std::vector<Action> &action);

 private:
  int m_uid;
  pxtnEvelist *m_evels;
  // std::vector<std::pair<int, PxtoneEditAction>> m_log;
  std::list<std::vector<Action>> m_uncommitted;
  int m_local_index;
  int m_remote_index;
};

#endif  // PXTONEACTIONSYNCHRONIZER_H
