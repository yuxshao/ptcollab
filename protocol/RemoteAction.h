#ifndef REMOTEACTION_H
#define REMOTEACTION_H
#include <editor/EditState.h>

#include <QDataStream>
#include <vector>

#include "protocol/PxtoneEditAction.h"
// TODO: Unfortunately we can't have a single message type because
// C++ doesn't support unions with nontrivial destructors (the vector below)
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

struct RemoteActionWithUid {
  RemoteAction action;
  qint64 uid;
};

QDataStream &operator<<(QDataStream &out, const RemoteActionWithUid &r);
QDataStream &operator>>(QDataStream &in, RemoteActionWithUid &r);
struct EditStateWithUid {
  EditState state;
  qint64 uid;
};

QDataStream &operator<<(QDataStream &out, const EditStateWithUid &r);
QDataStream &operator>>(QDataStream &in, EditStateWithUid &r);

// TODO: Lots of duplication in defining the stream operators. Would be nice to
// just say they all work as qint8s.
namespace FromServer {
enum MessageType {
  REMOTE_ACTION,
  EDIT_STATE,
  NEW_SESSION,
  DELETE_SESSION,
  ADD_UNIT
};
QDataStream &operator<<(QDataStream &out, const MessageType &r);
QDataStream &operator>>(QDataStream &in, MessageType &r);
}  // namespace FromServer

namespace FromClient {
enum MessageType { REMOTE_ACTION, EDIT_STATE, ADD_UNIT };
QDataStream &operator<<(QDataStream &out, const MessageType &r);
QDataStream &operator>>(QDataStream &in, MessageType &r);
}  // namespace FromClient

#endif  // REMOTEACTION_H
