#ifndef INTERVAL_H
#define INTERVAL_H

#include <QDataStream>
#include <optional>

int quantize(int v, int q);
struct Interval {
  qint32 start;
  qint32 end;

  bool contains(qint32 x) const { return (start <= x && x < end); }
  std::optional<int> position_along_interval(qint32 x) const {
    return (start <= x && x < end ? std::optional<int>(x - start)
                                  : std::nullopt);
  }

  qint32 length() const { return end - start; }
  bool empty() const { return end <= start; }
};
inline Interval interval_intersect(const Interval &a, const Interval &b) {
  return {qMax(a.start, b.start), qMin(a.end, b.end)};
}
inline Interval operator/(const Interval &a, const qreal b) {
  return {qint32(a.start / b), qint32(a.end / b)};
}
inline Interval operator*(const Interval &a, const qreal b) {
  return {qint32(a.start * b), qint32(a.end * b)};
}
QDataStream &operator<<(QDataStream &out, const Interval &a);
QDataStream &operator>>(QDataStream &in, Interval &a);
#endif  // INTERVAL_H
