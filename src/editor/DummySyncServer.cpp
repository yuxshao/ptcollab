#include "DummySyncServer.h"

#include <QDebug>
#include <QTimer>

DummySyncServer::DummySyncServer(PxtoneController *sync,
                                 float commit_lag_s, float mirror_lag_s)
    : m_sync(sync),
      m_commit_lag_s(commit_lag_s),
      m_mirror_lag_s(mirror_lag_s) {}

void DummySyncServer::receiveAction(
    const std::list<Action::Primitive> &action) {
  EditAction remote_action = m_sync->applyLocalAction(action);
  m_pending_commit.emplace_back(remote_action);
  m_pending_mirror.emplace_back(remote_action);
  QTimer::singleShot(1000 * m_commit_lag_s, [this]() {
    qDebug() << "Dummy commit";
    m_sync->applyRemoteAction(m_pending_commit.front(), 0);
    m_pending_commit.pop_front();
  });
  QTimer::singleShot(1000 * m_mirror_lag_s, [this]() {
    qDebug() << "Dummy mirror";
    m_sync->applyRemoteAction(m_pending_mirror.front(), 1);
    m_pending_mirror.pop_front();
  });
}

void DummySyncServer::receiveUndo() {
  QTimer::singleShot(1000 * m_commit_lag_s,
                     [this]() { m_sync->applyUndoRedo(UndoRedo::UNDO, 0); });
}

void DummySyncServer::receiveRedo() {
  QTimer::singleShot(1000 * m_commit_lag_s,
                     [this]() { m_sync->applyUndoRedo(UndoRedo::REDO, 1); });
}
