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

inline QDataStream &operator<<(QDataStream &out, const std::monostate &) {
  return out;
}
inline QDataStream &operator>>(QDataStream &in, std::monostate &) { return in; }

struct EditAction {
  qint64 idx;
  std::list<Action::Primitive> action;
};
inline QDataStream &operator<<(QDataStream &out, const EditAction &a) {
  out << a.idx << quint64(a.action.size());
  for (const auto &a : a.action) out << a;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, EditAction &a) {
  quint64 size;
  in >> a.idx >> size;
  if (in.status() != QDataStream::Ok) return in;
  for (size_t i = 0; i < size; ++i) {
    Action::Primitive action;
    in >> action;
    if (in.status() != QDataStream::Ok) return in;
    a.action.push_back(action);
  }
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
struct MoveUnit {
  qint32 unit_id;
  bool up;
};
inline QDataStream &operator<<(QDataStream &out, const MoveUnit &a) {
  out << a.unit_id << a.up;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, MoveUnit &a) {
  in >> a.unit_id >> a.up;
  return in;
}
inline QTextStream &operator<<(QTextStream &out, const MoveUnit &a) {
  out << "MoveUnit(" << a.unit_id << "," << a.up << ")";
  return out;
}

struct SetUnitName {
  qint32 unit_id;
  QString name;
};
inline QDataStream &operator<<(QDataStream &out, const SetUnitName &a) {
  out << a.unit_id << a.name;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, SetUnitName &a) {
  in >> a.unit_id >> a.name;
  return in;
}
inline QTextStream &operator<<(QTextStream &out, const SetUnitName &a) {
  out << "SetUnitName(" << a.name << ", " << a.unit_id << ")";
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

// Sort of elegant a change represented by same data as remove then add.
struct ChangeWoice {
  RemoveWoice remove;
  AddWoice add;
};
inline QDataStream &operator<<(QDataStream &out, const ChangeWoice &a) {
  out << a.remove << a.add;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, ChangeWoice &a) {
  in >> a.remove >> a.add;
  return in;
}
inline QTextStream &operator<<(QTextStream &out, const ChangeWoice &a) {
  out << "ChangeWoice(remove=" << a.remove << ", add=" << a.add << ")";
  return out;
}

inline QTextStream &operator<<(QTextStream &out, const MouseMeasureEdit &a) {
  out << "Measure(" << a.y << ")";
  return out;
}

inline QTextStream &operator<<(QTextStream &out, const MouseParamEdit &a) {
  out << "Param(" << a.current_param << ")";
  return out;
}

inline QTextStream &operator<<(QTextStream &out, const MouseKeyboardEdit &a) {
  out << "Keyboard(" << a.current_pitch << ")";
  return out;
}
inline QTextStream &operator<<(QTextStream &out, const EditState &a) {
  out << "EditState(c" << a.mouse_edit_state.current_clock << ", p"
      << a.mouse_edit_state.kind << ")";
  return out;
}

struct TempoChange {
  qint32 tempo;
};
inline QDataStream &operator<<(QDataStream &out, const TempoChange &a) {
  out << a.tempo;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, TempoChange &a) {
  in >> a.tempo;
  return in;
}
inline QTextStream &operator<<(QTextStream &out, const TempoChange &a) {
  out << "TempoChange(" << a.tempo << ")";
  return out;
}
struct BeatChange {
  qint32 beat;
};
inline QDataStream &operator<<(QDataStream &out, const BeatChange &a) {
  out << a.beat;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, BeatChange &a) {
  in >> a.beat;
  return in;
}
inline QTextStream &operator<<(QTextStream &out, const BeatChange &a) {
  out << "BeatChange(" << a.beat << ")";
  return out;
}

struct SetRepeatMeas {
  std::optional<qint32> meas;
};
inline QDataStream &operator<<(QDataStream &out, const SetRepeatMeas &a) {
  out << a.meas;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, SetRepeatMeas &a) {
  in >> a.meas;
  return in;
}
inline QTextStream &operator<<(QTextStream &out, const SetRepeatMeas &a) {
  out << "SetRepeatMeas(" << a.meas.value_or(-1) << ")";
  return out;
}

struct SetLastMeas {
  std::optional<qint32> meas;
};
inline QDataStream &operator<<(QDataStream &out, const SetLastMeas &a) {
  out << a.meas;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, SetLastMeas &a) {
  in >> a.meas;
  return in;
}
inline QTextStream &operator<<(QTextStream &out, const SetLastMeas &a) {
  out << "SetLastMeas(" << a.meas.value_or(-1) << ")";
  return out;
}

namespace Overdrive {
struct Set {
  qint32 ovdrv_no;
  qreal cut;
  qreal amp;
  qint32 group;
};
inline QDataStream &operator<<(QDataStream &out, const Set &a) {
  out << a.ovdrv_no << a.cut << a.amp << a.group;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, Set &a) {
  in >> a.ovdrv_no >> a.cut >> a.amp >> a.group;
  return in;
}
inline QTextStream &operator<<(QTextStream &out, const Set &a) {
  out << "Overdrive::Set(" << a.ovdrv_no << ", " << a.cut << ", " << a.amp
      << ", " << a.group << ")";
  return out;
}
struct Add : public std::monostate {};
inline QTextStream &operator<<(QTextStream &out, const Add &) {
  out << "Overdrive::Add()";
  return out;
}

struct Remove {
  qint32 ovdrv_no;
};
inline QDataStream &operator<<(QDataStream &out, const Remove &a) {
  out << a.ovdrv_no;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, Remove &a) {
  in >> a.ovdrv_no;
  return in;
}
inline QTextStream &operator<<(QTextStream &out, const Remove &a) {
  out << "Overdrive::Remove(" << a.ovdrv_no << ")";
  return out;
}

}  // namespace Overdrive

namespace Delay {

struct Set {
  qint32 delay_no;
  DELAYUNIT unit;
  qreal freq;
  qreal rate;
  qint32 group;
};
inline QDataStream &operator<<(QDataStream &out, const Set &a) {
  out << a.delay_no << a.unit << a.freq << a.rate << a.group;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, Set &a) {
  in >> a.delay_no >> a.unit >> a.freq >> a.rate >> a.group;
  return in;
}

inline QTextStream &operator<<(QTextStream &out, const Set &a) {
  out << "Delay::Set(" << a.delay_no << ", " << DELAYUNIT_name(a.unit) << ", "
      << a.freq << ", " << a.rate << ", " << a.group << ")";
  return out;
}
}  // namespace Delay

namespace Woice {

struct Key {
  qint32 key;
};
inline QDataStream &operator<<(QDataStream &out, const Key &a) {
  out << a.key;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, Key &a) {
  in >> a.key;
  return in;
}

struct Name {
  QString name;
};
inline QDataStream &operator<<(QDataStream &out, const Name &a) {
  out << a.name;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, Name &a) {
  in >> a.name;
  return in;
}
struct Flag {
  enum { LOOP, BEATFIT } flag;
  bool set;
};
inline QDataStream &operator<<(QDataStream &out, const Flag &a) {
  out << a.flag << a.set;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, Flag &a) {
  in >> a.flag >> a.set;
  return in;
}

using Setting = std::variant<Name, Flag, Key>;
struct Set {
  qint32 id;
  Setting setting;
};
inline QDataStream &operator<<(QDataStream &out, const Set &a) {
  out << a.id << a.setting;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, Set &a) {
  in >> a.id >> a.setting;
  return in;
}

inline QTextStream &operator<<(QTextStream &out, const Set &a) {
  out << "Woice::Set(" << a.id << ")";
  return out;
}
}  // namespace Woice

using ClientAction =
    std::variant<EditAction, EditState, UndoRedo, AddUnit, RemoveUnit, MoveUnit,
                 AddWoice, RemoveWoice, ChangeWoice, TempoChange, BeatChange,
                 SetRepeatMeas, SetLastMeas, SetUnitName, Overdrive::Add,
                 Overdrive::Set, Overdrive::Remove, Delay::Set, Woice::Set>;
inline bool clientActionShouldBeRecorded(const ClientAction &a) {
  bool ret;
  std::visit(overloaded{[&ret](const EditState &) { ret = false; },
                        [&ret](const auto &) { ret = true; }},
             a);
  return ret;
}

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
    std::visit(overloaded{[&ret](const ClientAction &a) {
                            ret = clientActionShouldBeRecorded(a);
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

#endif  // REMOTEACTION_H
