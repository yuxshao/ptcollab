#ifndef REMOTEACTION_H
#define REMOTEACTION_H
#include <editor/EditState.h>

#include <QDataStream>
#include <variant>
#include <vector>

#include "protocol/Data.h"
#include "protocol/PxtoneEditAction.h"
#include "protocol/SerializeVariant.h"

// boilerplate for std::visit
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

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
template <typename T>
QDataStream &read_as_qint8(QDataStream &in, T &x) {
  qint8 v;
  in >> v;
  x = T(v);
  return in;
}

inline QDataStream &operator<<(QDataStream &out, const UndoRedo &a) {
  out << qint8(a);
  return out;
}
inline QDataStream &operator>>(QDataStream &in, UndoRedo &a) {
  read_as_qint8(in, a);
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

struct RemoveUnit {
  qint32 unit_id;
};
inline QDataStream &operator<<(QDataStream &out, const RemoveUnit &a) {
  out << a.unit_id;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, RemoveUnit &a) {
  in >> a.unit_id;
  return in;
}

struct AddWoice {
  pxtnWOICETYPE type;
  QString name;
  QByteArray data;
};
inline QDataStream &operator<<(QDataStream &out, const AddWoice &a) {
  out << (qint8)a.type << a.name << a.data;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, AddWoice &a) {
  read_as_qint8(in, a.type);
  in >> a.name >> a.data;
  return in;
}

struct RemoveWoice {
  qint32 id;
  QString name;
};
inline QDataStream &operator<<(QDataStream &out, const RemoveWoice &a) {
  out << a.id << a.name;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, RemoveWoice &a) {
  in >> a.id >> a.name;
  return in;
}

using ClientAction = std::variant<EditAction, EditState, UndoRedo, AddUnit,
                                  RemoveUnit, AddWoice, RemoveWoice>;

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
    // TODO: instead of using this to track history, have the broadcast server
    // update its own internal state (similar to the synchronizer) and return
    // that
    bool ret;
    std::visit(
        overloaded{[&ret](const ClientAction &a) {
                     std::visit(
                         overloaded{[&ret](const EditState &) { ret = false; },
                                    [&ret](const auto &) { ret = true; }},
                         a);
                   },
                   [&ret](const auto &) { ret = true; }},
        action);
    return ret;
  }
};

inline QDataStream &operator<<(QDataStream &out, const ServerAction &a) {
  out << a.uid << a.action;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, ServerAction &a) {
  in >> a.uid >> a.action;
  return in;
}

#endif  // REMOTEACTION_H
