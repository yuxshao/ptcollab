#include "DummySyncServer.h"

#include <QDebug>
#include <QTimer>

DummySyncServer::DummySyncServer(PxtoneActionSynchronizer *sync,
                                 float commit_lag_s, float mirror_lag_s)
    : m_sync(sync),
      m_commit_lag_s(commit_lag_s),
      m_mirror_lag_s(mirror_lag_s) {}

void DummySyncServer::receiveAction(const std::vector<Action> &action) {
  RemoteAction remote_action = m_sync->applyLocalAction(action);
  m_pending_commit.emplace_back(remote_action);
  m_pending_mirror.emplace_back(remote_action);
  QTimer::singleShot(1000 * m_commit_lag_s, [this]() {
    qDebug() << "Dummy commit";
    m_sync->applyRemoteAction(0, m_pending_commit.front());
    m_pending_commit.pop_front();
  });
  /*QTimer::singleShot(1000000 * m_mirror_lag_s, [this]() {
    qDebug() << "Dummy mirror";
    m_sync->applyRemoteAction(1, m_pending_mirror.front());
    m_pending_mirror.pop_front();
  });*/
}

void DummySyncServer::receiveUndo() {
  QTimer::singleShot(1000 * m_commit_lag_s, [this]() {
    m_sync->applyRemoteAction(0, m_sync->getUndo());
  });
}

void DummySyncServer::receiveRedo() {
  QTimer::singleShot(1000 * m_commit_lag_s, [this]() {
    m_sync->applyRemoteAction(0, m_sync->getRedo());
  });
}
