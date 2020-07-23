#ifndef EDITSTATE_H
#define EDITSTATE_H
#include <pxtone/pxtnEvelist.h>

#include <QObject>
#include <optional>
#include <variant>

#include "Interval.h"

struct MouseKeyboardEdit {
  qint32 start_pitch;
  qint32 current_pitch;
};
inline QDataStream &operator<<(QDataStream &out, const MouseKeyboardEdit &a) {
  out << a.start_pitch << a.current_pitch;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, MouseKeyboardEdit &a) {
  in >> a.start_pitch >> a.current_pitch;
  return in;
}

struct MouseParamEdit {
  // qint32 param_idx;
  qint32 start_param;
  qint32 current_param;
};
inline QDataStream &operator<<(QDataStream &out, const MouseParamEdit &a) {
  out << a.start_param << a.current_param;
  return out;
}
inline QDataStream &operator>>(QDataStream &in, MouseParamEdit &a) {
  in >> a.start_param >> a.current_param;
  return in;
}

struct MouseEditState {
  enum Type { Nothing, Seek, SetNote, SetOn, DeleteNote, DeleteOn, Select };
  Type type;
  qreal base_velocity;
  qint32 last_pitch;
  qint32 start_clock;
  qint32 current_clock;
  std::variant<MouseKeyboardEdit, MouseParamEdit> kind;
  std::optional<Interval> selection;
  Interval clock_int(qint32 quantize) const;
  MouseEditState();
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
bool operator==(const Scale &x, const Scale &y);

struct EditState {
  MouseEditState mouse_edit_state;
  Scale scale;
  int m_current_unit_id;
  int m_current_param_kind_idx;
  int m_quantize_clock_idx;
  int m_quantize_pitch_idx;
  bool m_follow_playhead;
  EditState();
};
QDataStream &operator<<(QDataStream &out, const EditState &a);
QDataStream &operator>>(QDataStream &in, EditState &a);

#endif  // EDITSTATE_H
