#ifndef DUMMYSYNCSERVER_H
#define DUMMYSYNCSERVER_H

#include <list>

#include "PxtoneActionSynchronizer.h"

// Commit actions x seconds after they were written.
// Pretend there's another user mirroring your exact actions y seconds after
// they were written.
class DummySyncServer {
 public:
  // TODO: What if sync is destroyed?
  DummySyncServer(PxtoneActionSynchronizer *sync, float commit_lag_s,
                  float mirror_lag_s);

  void receiveAction(const std::vector<Action> &action);
  void receiveUndo();
  void receiveRedo();

 private:
  PxtoneActionSynchronizer *m_sync;
  float m_commit_lag_s;
  float m_mirror_lag_s;
  std::list<RemoteAction> m_pending_commit;
  std::list<RemoteAction> m_pending_mirror;
};

#endif  // DUMMYSYNCSERVER_H
