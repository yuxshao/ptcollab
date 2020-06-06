#ifndef REMOTEACTION_H
#define REMOTEACTION_H
#include <QDataStream>
#include <vector>

#include "protocol/PxtoneEditAction.h"
class RemoteAction {
  // TODO: Honestly UNDO REDO do not need to have an idx. the idx is just to
  // identify when a list of local actions has been committed. And UNDO REDO are
  // only applied from remote.
 public:
  enum Type { ACTION, UNDO, REDO };
  Type type;
  qint64 idx;
  std::vector<Action> action;
};
QDataStream &operator<<(QDataStream &out, const RemoteAction &r);
QDataStream &operator>>(QDataStream &in, RemoteAction &r);
#endif  // REMOTEACTION_H
