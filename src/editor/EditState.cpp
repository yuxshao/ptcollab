#include "EditState.h"

#include <QDataStream>

#include "protocol/SerializeVariant.h"
#include "pxtone/pxtnEvelist.h"

Interval MouseEditState::clock_int(int q) const {
  int begin = std::min(start_clock, current_clock);
  int end = std::max(start_clock, current_clock);
  return {quantize(begin, q), quantize(end, q) + q};
}
EditState::EditState()
    : mouse_edit_state({MouseEditState::Type::Nothing, EVENTDEFAULT_VELOCITY,
                        EVENTDEFAULT_KEY, 0, 0, 0, 0, std::nullopt}),
      scale(),
      m_current_unit_id(0),
      m_quantize_clock(1),
      m_quantize_pitch(1) {}

QDataStream &operator<<(QDataStream &out, const MouseEditState &a) {
  return (out << qint8(a.type) << a.base_velocity << a.last_pitch
              << a.start_clock << a.current_clock << a.start_pitch
              << a.current_pitch << a.selection);
}

QDataStream &operator>>(QDataStream &in, MouseEditState &a) {
  qint8 type_int;
  in >> type_int >> a.base_velocity >> a.last_pitch >> a.start_clock >>
      a.current_clock >> a.start_pitch >> a.current_pitch >> a.selection;
  a.type = MouseEditState::Type(type_int);
  return in;
}

QDataStream &operator<<(QDataStream &out, const Scale &a) {
  return (out << a.clockPerPx << a.pitchPerPx << a.noteHeight << a.pitchOffset);
}

QDataStream &operator>>(QDataStream &in, Scale &a) {
  return (in >> a.clockPerPx >> a.pitchPerPx >> a.noteHeight >> a.pitchOffset);
}

QDataStream &operator<<(QDataStream &out, const EditState &a) {
  return (out << a.mouse_edit_state << a.scale << a.m_current_unit_id
              << a.m_quantize_clock << a.m_quantize_pitch);
}

QDataStream &operator>>(QDataStream &in, EditState &a) {
  return (in >> a.mouse_edit_state >> a.scale >> a.m_current_unit_id >>
          a.m_quantize_clock >> a.m_quantize_pitch);
}
