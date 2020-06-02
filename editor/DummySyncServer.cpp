#include "DummySyncServer.h"

#include <QDebug>
#include <QTimer>

DummySyncServer::DummySyncServer(PxtoneActionSynchronizer *sync,
                                 float commit_lag_s, float mirror_lag_s)
    : m_sync(sync),
      m_commit_lag_s(commit_lag_s),
      m_mirror_lag_s(mirror_lag_s) {}

void DummySyncServer::receiveAction(const std::vector<Action> &action) {
  int idx = m_sync->applyLocalActionAndGetIdx(action);
  m_pending_commit.emplace_back(idx, action);
  m_pending_mirror.emplace_back(idx, action);
  QTimer::singleShot(1000 * m_commit_lag_s, [this]() {
    qDebug() << "Dummy commit";
    auto [idx, action] = m_pending_commit.front();
    m_sync->applyRemoteAction(0, idx, action);
    m_pending_commit.pop_front();
  });
  QTimer::singleShot(1000 * m_mirror_lag_s, [this]() {
    qDebug() << "Dummy mirror";
    auto [idx, action] = m_pending_mirror.front();
    m_sync->applyRemoteAction(1, idx, action);
    m_pending_mirror.pop_front();
  });
}
