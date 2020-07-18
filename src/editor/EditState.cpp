#include "EditState.h"

#include <QDataStream>

#include "protocol/SerializeVariant.h"
#include "pxtone/pxtnEvelist.h"

bool operator==(const Scale &x, const Scale &y) {
  return x.clockPerPx == y.clockPerPx && x.pitchPerPx == y.pitchPerPx &&
         x.pitchOffset == y.pitchOffset && x.noteHeight == y.noteHeight;
}

Interval MouseEditState::clock_int(int q) const {
  int begin = std::min(start_clock, current_clock);
  int end = std::max(start_clock, current_clock);
  return {quantize(begin, q), quantize(end, q) + q};
}

MouseEditState::MouseEditState()
    : type(MouseEditState::Type::Nothing),
      base_velocity(EVENTDEFAULT_VELOCITY),
      last_pitch(EVENTDEFAULT_KEY),
      start_clock(0),
      current_clock(0),
      kind(MouseKeyboardEdit({0, 0})),
      selection(std::nullopt) {}

QDataStream &operator<<(QDataStream &out, const MouseEditState &a) {
  return (out << qint8(a.type) << a.base_velocity << a.last_pitch
              << a.start_clock << a.current_clock << a.kind << a.selection);
}

QDataStream &operator>>(QDataStream &in, MouseEditState &a) {
  qint8 type_int;
  in >> type_int >> a.base_velocity >> a.last_pitch >> a.start_clock >>
      a.current_clock >> a.kind >> a.selection;
  a.type = MouseEditState::Type(type_int);
  return in;
}

QDataStream &operator<<(QDataStream &out, const Scale &a) {
  return (out << a.clockPerPx << a.pitchPerPx << a.noteHeight << a.pitchOffset);
}

QDataStream &operator>>(QDataStream &in, Scale &a) {
  return (in >> a.clockPerPx >> a.pitchPerPx >> a.noteHeight >> a.pitchOffset);
}

EditState::EditState()
    : mouse_edit_state(),
      scale(),
      m_current_unit_id(0),
      m_current_param_kind_idx(0),
      m_quantize_clock_idx(0),
      m_quantize_pitch_idx(0) {}

QDataStream &operator<<(QDataStream &out, const EditState &a) {
  return (out << a.mouse_edit_state << a.scale << a.m_current_unit_id
              << a.m_current_param_kind_idx << a.m_quantize_clock_idx
              << a.m_quantize_pitch_idx);
}

QDataStream &operator>>(QDataStream &in, EditState &a) {
  return (in >> a.mouse_edit_state >> a.scale >> a.m_current_unit_id >>
          a.m_current_param_kind_idx >> a.m_quantize_clock_idx >>
          a.m_quantize_pitch_idx);
}
