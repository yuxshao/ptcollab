#include "RemoteAction.h"
QDataStream &operator<<(QDataStream &out, const RemoteAction &r) {
  out << qint8(r.type) << r.idx << quint64(r.action.size());
  for (const Action &a : r.action) out << a;
  return out;
}
QDataStream &operator>>(QDataStream &in, RemoteAction &r) {
  quint64 size;
  qint8 type_i;
  in >> type_i >> r.idx >> size;
  r.type = RemoteAction::Type(type_i);
  r.action.resize(size);
  for (size_t i = 0; i < size; ++i) in >> r.action[i];
  return in;
}
