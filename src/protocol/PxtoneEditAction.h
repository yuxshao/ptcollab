#ifndef PXTONEEDITACTION_H
#define PXTONEEDITACTION_H

#include <QDataStream>
#include <QDebug>
#include <QList>
#include <optional>
#include <set>
#include <vector>

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

// Because unit nos are fixed in pxtone, we have to maintain this mapping so
// that actions are still valid past unit additions / deletions / moves.
class UnitIdMap {
 public:
  UnitIdMap(const pxtnService *pxtn) : m_next_id(0) {
    for (int32_t i = 0; i < pxtn->Unit_Num(); ++i) addUnit();
  }
  qint32 numUnits() const { return m_no_to_id.size(); }
  qint32 noToId(qint32 no) const { return m_no_to_id[no]; };
  std::optional<qint32> idToNo(qint32 id) const {
    auto it = m_id_to_no.find(id);
    if (it == m_id_to_no.end()) return std::nullopt;
    return qint32(it->second);
  };
  void addUnit() {
    qint32 id = m_next_id++;
    size_t no = m_no_to_id.size();

    // equiv: m_no_to_id[no] = id;
    m_no_to_id.push_back(id);
    m_id_to_no[id] = no;

    // qDebug() << QVector<qint32>(m_no_to_id.begin(), m_no_to_id.end())
    //         << QMap<qint32, qint32>(m_id_to_no);
  }
  void removeUnit(size_t no) {
    qint32 id = m_no_to_id[no];
    m_no_to_id.erase(m_no_to_id.begin() + no);
    m_id_to_no.erase(id);
    for (auto &it : m_id_to_no)
      if (it.second > no) --it.second;

    // qDebug() << QVector<qint32>(m_no_to_id.begin(), m_no_to_id.end())
    //         << QMap<qint32, size_t>(m_id_to_no);
  }
  // TODO: move unit

 private:
  int m_next_id;
  std::map<qint32, size_t> m_id_to_no;
  std::vector<qint32> m_no_to_id;
};

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
