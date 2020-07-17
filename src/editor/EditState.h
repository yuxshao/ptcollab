#ifndef EDITSTATE_H
#define EDITSTATE_H
#include <QObject>
#include <optional>

#include "Interval.h"

struct MouseEditState {
  enum Type { Nothing, Seek, SetNote, SetOn, DeleteNote, DeleteOn, Select };
  Type type;
  qreal base_velocity;
  qint32 last_pitch;
  qint32 start_clock;
  qint32 start_pitch;
  qint32 current_clock;
  qint32 current_pitch;
  std::optional<Interval> selection;
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
  int m_quantize_clock_idx;
  int m_quantize_pitch_idx;
  EditState();
};
QDataStream &operator<<(QDataStream &out, const EditState &a);
QDataStream &operator>>(QDataStream &in, EditState &a);

#endif  // EDITSTATE_H
