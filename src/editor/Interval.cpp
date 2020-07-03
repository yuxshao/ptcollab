#include "Interval.h"

int quantize(int v, int q) { return (v / q) * q; }

QDataStream &operator<<(QDataStream &out, const Interval &a) {
  return (out << a.start << a.end);
}

QDataStream &operator>>(QDataStream &in, Interval &a) {
  return (in >> a.start >> a.end);
}
