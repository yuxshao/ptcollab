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
  // TODO: Figure out where to call sth undo vs. reverse
  std::list<Action::Primitive> reverse;
  LoggedAction(qint64 uid, qint64 idx,
               const std::list<Action::Primitive> &reverse)
      : state(DONE), uid(uid), idx(idx), reverse(reverse) {}
};

class PxtoneActionSynchronizer : public QObject {
  Q_OBJECT
 public:
  PxtoneActionSynchronizer(int uid, pxtnService *pxtn, QObject *parent);

  // rename to applyAndGetEditAction and applyEditAction
  EditAction applyLocalAction(const std::list<Action::Primitive> &action);
  void setUid(qint64 uid);
  qint64 uid();
  const UnitIdMap &unitIdMap() { return m_unit_id_map; }
  void resetUnitIdMap() { m_unit_id_map = UnitIdMap(m_pxtn); }
  bool applyAddUnit(const AddUnit &a, qint64 uid);
  bool applyAddWoice(const AddWoice &a, qint64 uid);
  bool applyRemoveWoice(const RemoveWoice &a, qint64 uid);
  bool applyChangeWoice(const ChangeWoice &a, qint64 uid);

 public slots:
  // Maybe these types could be grouped.
  void applyRemoteAction(const EditAction &a, qint64 uid);
  void applyUndoRedo(const UndoRedo &r, qint64 uid);
  void applyRemoveUnit(const RemoveUnit &a, qint64 uid);

 signals:
  void measureNumChanged();

 private:
  qint64 m_uid;
  pxtnService *m_pxtn;
  std::vector<LoggedAction> m_log;
  std::list<std::list<Action::Primitive>> m_uncommitted;
  UnitIdMap m_unit_id_map;
  int m_remote_index;
};

#endif  // PXTONEACTIONSYNCHRONIZER_H
