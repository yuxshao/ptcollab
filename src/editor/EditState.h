#ifndef EDITSTATE_H
#define EDITSTATE_H
#include <QObject>
int quantize(int v, int q);
struct Interval {
  qint32 start;
  qint32 end;

  bool contains(qint32 x) const { return (start <= x && x < end); }

  qint32 length() const { return end - start; }
  bool empty() const { return end <= start; }
};
inline Interval interval_intersect(const Interval &a, const Interval &b) {
  return {std::max(a.start, b.start), std::min(a.end, b.end)};
}
QDataStream &operator<<(QDataStream &out, const Interval &a);
QDataStream &operator>>(QDataStream &in, Interval &a);
struct MouseEditState {
  enum Type { Nothing, Seek, SetNote, SetOn, DeleteNote, DeleteOn };
  Type type;
  qint32 base_velocity;
  qint32 start_clock;
  qint32 start_pitch;
  qint32 current_clock;
  qint32 current_pitch;
  Interval clock_int(qint32 quantize) const;
};
QDataStream &operator<<(QDataStream &out, const MouseEditState &a);
QDataStream &operator>>(QDataStream &in, MouseEditState &a);

constexpr int PITCH_PER_KEY = 256;
constexpr int EVENTMAX_KEY = 135 * PITCH_PER_KEY;
constexpr int EVENTMIN_KEY = 46 * PITCH_PER_KEY;
struct Scale {
  qreal clockPerPx;
  qreal pitchPerPx;
  qint32 pitchOffset;
  qint32 noteHeight;

  Scale()
      : clockPerPx(10),
        pitchPerPx(32),
        pitchOffset(EVENTMAX_KEY),
        noteHeight(5) {}
  qreal pitchToY(qreal pitch) const {
    return (pitchOffset - pitch) / pitchPerPx;
  }
  qreal pitchOfY(qreal y) const { return pitchOffset - y * pitchPerPx; }
};
QDataStream &operator<<(QDataStream &out, const Scale &a);
QDataStream &operator>>(QDataStream &in, Scale &a);

struct EditState {
  MouseEditState mouse_edit_state;
  Scale scale;
  int m_current_unit_id;
  int m_quantize_clock;
  int m_quantize_pitch;
  EditState();
};
QDataStream &operator<<(QDataStream &out, const EditState &a);
QDataStream &operator>>(QDataStream &in, EditState &a);

#endif  // EDITSTATE_H
