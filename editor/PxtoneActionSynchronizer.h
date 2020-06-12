#ifndef PXTONEACTIONSYNCHRONIZER_H
#define PXTONEACTIONSYNCHRONIZER_H
#include <QObject>
#include <list>

#include "protocol/PxtoneEditAction.h"
#include "protocol/RemoteAction.h"

// Okay, I give up on eager undo. It's just way too hard to roll back an undo
// from the local branch.

struct LoggedAction {
  enum UndoState { DONE, UNDONE, GONE };
  UndoState state;
  qint64 uid;
  qint64 idx;
  std::vector<Action>
      reverse;  // TODO: Figure out where to call sth undo vs. reverse
  LoggedAction(qint64 uid, qint64 idx, const std::vector<Action> &reverse)
      : state(DONE), uid(uid), idx(idx), reverse(reverse) {}
};

class PxtoneActionSynchronizer : public QObject {
  Q_OBJECT
 public:
  PxtoneActionSynchronizer(int uid, pxtnService *pxtn, QObject *parent);

  RemoteAction applyLocalAction(const std::vector<Action> &action);
  // Not a great API. One applies but the other just gets actions?
  // And you have to make sure the caller sends them over to the server in the
  // right order.
  RemoteAction getUndo();
  RemoteAction getRedo();
  void setUid(qint64 uid);
  qint64 uid();

 public slots:
  void applyRemoteAction(const RemoteActionWithUid &);
  void applyAddUnit(qint32 woice_id, QString woice_name, QString unit_name,
                    qint64 uid);
 signals:
  void measureNumChanged();

 private:
  qint64 m_uid;
  pxtnService *m_pxtn;
  std::vector<LoggedAction> m_log;
  std::list<std::vector<Action>> m_uncommitted;
  int m_local_index;
  int m_remote_index;
};

#endif  // PXTONEACTIONSYNCHRONIZER_H
