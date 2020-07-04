#ifndef PXTONEEDITACTION_H
#define PXTONEEDITACTION_H

#include <QDataStream>
#include <QDebug>
#include <QList>
#include <optional>
#include <set>
#include <vector>

#include "UnitIdMap.h"
#include "protocol/SerializeVariant.h"
#include "pxtone/pxtnService.h"

// boilerplate for std::visit
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template <typename T>
inline QDebug &operator<<(QDebug &out, const T &a) {
  QString s;
  QTextStream ts(&s);
  ts << a;
  out << s;
  return out;
}

template <typename T>
QDataStream &read_as_qint8(QDataStream &in, T &x) {
  qint8 v;
  in >> v;
  x = T(v);
  return in;
}

namespace Action {

// Assumes this is overwriting nothing
struct Add {
  qint32 value;
};
inline QTextStream &operator<<(QTextStream &out, const Add &a) {
  out << "add(" << a.value << ")";
  return out;
}
struct Delete {
  qint32 end_clock;
};
inline QTextStream &operator<<(QTextStream &out, const Delete &a) {
  out << "Delete(" << a.end_clock << ")";
  return out;
}
struct Shift {
  qint32 end_clock;
  qint32 offset;
};
inline QTextStream &operator<<(QTextStream &out, const Shift &a) {
  out << "Shift(" << a.end_clock << ", " << a.offset << ")";
  return out;
}
struct Primitive {
  EVENTKIND kind;
  qint32 unit_id;
  qint32 start_clock;
  std::variant<Add, Delete, Shift> type;
};
inline QTextStream &operator<<(QTextStream &out, const Primitive &a) {
  out << "Primitive(" << EVENTKIND_names[a.kind] << ", u" << a.unit_id << ", "
      << a.start_clock << ", " << a.type << ")";
  return out;
}
QDataStream &operator<<(QDataStream &out, const Primitive &a);
QDataStream &operator>>(QDataStream &in, Primitive &a);

// You have to compute the undo at the time the original action was applied in
// the case of collaborative editing. You can't compute it beforehand.
std::list<Primitive> apply_and_get_undo(const std::list<Primitive> &actions,
                                        pxtnService *pxtn, bool *widthChanged,
                                        const UnitIdMap &map);
}  // namespace Action

#endif  // PXTONEEDITACTION_H
