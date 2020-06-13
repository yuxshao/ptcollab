#ifndef REMOTEACTION_H
#define REMOTEACTION_H
#include <editor/EditState.h>

#include <QDataStream>
#include <variant>
#include <vector>

#include "protocol/PxtoneEditAction.h"
#include "protocol/SerializeVariant.h"

inline QDataStream &operator<<(QDataStream &out, const std::monostate &) {
  return out;
}
inline QDataStream &operator>>(QDataStream &in, std::monostate &) { return in; }

struct EditAction {
  qint64 idx;
  std::vector<Action> action;
};
inline QDataStream &operator<<(QDataStream &out, const EditAction &r) {
  out << r.idx << quint64(r.action.size());
  for (const auto &a : r.action) out << a;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, EditAction &r) {
  quint64 size;
  in >> r.idx >> size;
  r.action.resize(size);
  for (size_t i = 0; i < size; ++i) in >> r.action[i];
  return in;
}

enum UndoRedo { UNDO, REDO };

inline QDataStream &operator<<(QDataStream &out, const UndoRedo &a) {
  out << (qint8)a;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, UndoRedo &a) {
  in >> *(qint8 *)(&a);
  return in;
}
struct AddUnit {
  qint32 woice_id;
  QString woice_name;
  QString unit_name;
};
inline QDataStream &operator<<(QDataStream &out, const AddUnit &a) {
  out << a.woice_id << a.woice_name << a.unit_name;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, AddUnit &a) {
  in >> a.woice_id >> a.woice_name >> a.unit_name;
  return in;
}

using ClientAction = std::variant<EditAction, EditState, UndoRedo, AddUnit>;
inline QDataStream &operator<<(QDataStream &out, const ClientAction &a) {
  return serializeVariant(out, a);
}
inline QDataStream &operator>>(QDataStream &in, ClientAction &a) {
  return deserializeVariant(in, a);
};

struct NewSession {
  QString username;
};
inline QDataStream &operator<<(QDataStream &out, const NewSession &a) {
  out << a.username;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, NewSession &a) {
  in >> a.username;
  return in;
};
struct DeleteSession : public std::monostate {};
struct ServerAction {
  qint64 uid;
  std::variant<ClientAction, NewSession, DeleteSession> action;
  bool shouldBeRecorded() const {
    // TODO: match on variant, only return editaction
    // TODO: instead of using this to track history, have the broadcast server
    // update its own internal state (similar to the synchronizer) and return
    // that
    return true;
  }
};

inline QDataStream &operator<<(QDataStream &out, const ServerAction &a) {
  out << a.uid;
  serializeVariant(out, a.action);
  return out;
}
inline QDataStream &operator>>(QDataStream &in, ServerAction &a) {
  in >> a.uid;
  deserializeVariant(in, a.action);
  return in;
}

#endif  // REMOTEACTION_H
