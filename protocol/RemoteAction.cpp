#include "RemoteAction.h"

#include <QDebug>
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

QDataStream &operator<<(QDataStream &out, const RemoteActionWithUid &r) {
  return (out << r.action << r.uid);
}
QDataStream &operator>>(QDataStream &in, RemoteActionWithUid &r) {
  return (in >> r.action >> r.uid);
}

QDataStream &operator<<(QDataStream &out, const EditStateWithUid &r) {
  return (out << r.state << r.uid);
}
QDataStream &operator>>(QDataStream &in, EditStateWithUid &r) {
  return (in >> r.state >> r.uid);
}

QDataStream &operator<<(QDataStream &out, const MessageType &m) {
  return (out << qint8(m));
}
QDataStream &operator>>(QDataStream &in, MessageType &m) {
  qint8 type_int;
  in >> type_int;
  m = MessageType(type_int);
  return in;
}
