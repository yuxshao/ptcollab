#ifndef REMOTEACTION_H
#define REMOTEACTION_H
#include <editor/EditState.h>

#include <QCryptographicHash>
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
inline QDataStream &operator<<(QDataStream &out, const EditAction &a) {
  out << a.idx << quint64(a.action.size());
  for (const auto &a : a.action) out << a;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, EditAction &a) {
  quint64 size;
  in >> a.idx >> size;
  a.action.resize(size);
  for (size_t i = 0; i < size; ++i) in >> a.action[i];
  return in;
}
inline QTextStream &operator<<(QTextStream &out, const EditAction &a) {
  out << "EditAction(idx=" << a.idx << ", "
      << "size=" << a.action.size() << ")";
  return out;
}

enum UndoRedo { UNDO, REDO };

inline QTextStream &operator<<(QTextStream &out, const UndoRedo &a) {
  out << "UndoRedo(";
  switch (a) {
    case UNDO:
      out << "UNDO";
      break;
    case REDO:
      out << "REDO";
      break;
  }
  out << ")";
  return out;
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
inline QTextStream &operator<<(QTextStream &out, const AddUnit &a) {
  out << "AddUnit(woice_id=" << a.woice_id << ", woice_name=" << a.woice_name
      << ", unit_name=" << a.unit_name << ")";
  return out;
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
inline QTextStream &operator<<(QTextStream &out, const RemoveUnit &a) {
  out << "RemoveUnit(unit_id=" << a.unit_id << ")";
  return out;
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

static const char *pxtnWOICETYPE_names[] = {"None", "PCM", "PTV", "PTN",
                                            "OGGV"};

inline QByteArray hashData(const QByteArray &data) {
  QByteArray hash =
      QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex();
  hash.truncate(16);
  return hash;
}

inline QTextStream &operator<<(QTextStream &out, const AddWoice &a) {
  out << "AddWoice(unit_id=" << pxtnWOICETYPE_names[a.type]
      << ", name=" << a.name << ", data=(" << a.data.length() << ")"
      << hashData(a.data) << ")";
  return out;
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
inline QTextStream &operator<<(QTextStream &out, const RemoveWoice &a) {
  out << "RemoveWoice(id=" << a.id << ", name=" << a.name << ")";
  return out;
}

inline QTextStream &operator<<(QTextStream &out, const EditState &a) {
  out << "EditState(c" << a.mouse_edit_state.current_clock << ", p"
      << a.mouse_edit_state.current_pitch << ")";
  return out;
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
inline QTextStream &operator<<(QTextStream &out, const NewSession &a) {
  out << "NewSession(username=" << a.username << ")";
  return out;
}

struct DeleteSession : public std::monostate {};
inline QTextStream &operator<<(QTextStream &out, const DeleteSession &) {
  out << "DeleteSession()";
  return out;
}
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
inline QTextStream &operator<<(QTextStream &out, const ServerAction &a) {
  out << "ServerAction(u" << a.uid << ", " << a.action << ")";
  return out;
}

template <typename T>
inline QDebug &operator<<(QDebug &out, const T &a) {
  QString s;
  QTextStream ts(&s);
  ts << a;
  out << s;
  return out;
}

#endif  // REMOTEACTION_H
