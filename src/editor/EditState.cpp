#include "EditState.h"

#include <QDataStream>

#include "ComboOptions.h"
#include "protocol/SerializeVariant.h"
#include "pxtone/pxtnEvelist.h"

QDataStream &operator>>(QDataStream &in, Input::Event::On &a) {
  return (in >> a.key >> a.vel);
}

QDataStream &operator<<(QDataStream &out, const Input::Event::On &a) {
  return (out << a.key << a.vel);
}

QDataStream &operator>>(QDataStream &in, Input::State::On &a) {
  return (in >> a.start_clock >> a.on);
}

QDataStream &operator<<(QDataStream &out, const Input::State::On &a) {
  return (out << a.start_clock << a.on);
}

bool operator==(const Scale &x, const Scale &y) {
  return x.clockPerPx == y.clockPerPx && x.pitchPerPx == y.pitchPerPx &&
         x.pitchOffset == y.pitchOffset && x.noteHeight == y.noteHeight;
}

Interval MouseEditState::clock_int(int q) const {
  int begin = std::min(start_clock, current_clock);
  int end = std::max(start_clock, current_clock);
  return {quantize(begin, q), quantize(end, q) + q};
}

Interval MouseEditState::clock_int_short(int q) const {
  int begin = std::min(start_clock, current_clock);
  int end = std::max(start_clock, current_clock);
  return {quantize(begin, q), quantize(end, q)};
}

MouseEditState::MouseEditState()
    : type(MouseEditState::Type::Nothing),
      base_velocity(EVENTDEFAULT_VELOCITY),
      last_pitch(EVENTDEFAULT_KEY),
      start_clock(0),
      current_clock(0),
      kind(MouseKeyboardEdit({MouseMainKeyboard{0}, 0})),
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

int EditState::current_param_kind_idx() const {
  return ((m_current_param_kind_idx % paramOptions().size()) +
          paramOptions().size()) %
         paramOptions().size();
}

EditState::EditState()
    : mouse_edit_state(),
      scale(),
      viewport(),
      m_pinned_unit_ids(),
      m_current_unit_id(0),
      m_current_woice_id(0),
      m_current_param_kind_idx(0),
      m_quantize_clock_idx(0),
      m_quantize_pitch_denom(12),
      m_follow_playhead(FollowPlayhead::None) {}

QDataStream &operator<<(QDataStream &out, const EditState &a) {
  return (out << a.mouse_edit_state << a.scale << a.viewport
              << a.m_pinned_unit_ids << a.m_current_unit_id
              << a.m_current_woice_id << a.m_current_param_kind_idx
              << a.m_quantize_clock_idx << a.m_quantize_pitch_denom
              << a.m_follow_playhead << a.m_input_state);
}

QDataStream &operator>>(QDataStream &in, EditState &a) {
  return (in >> a.mouse_edit_state >> a.scale >> a.viewport >>
          a.m_pinned_unit_ids >> a.m_current_unit_id >> a.m_current_woice_id >>
          a.m_current_param_kind_idx >> a.m_quantize_clock_idx >>
          a.m_quantize_pitch_denom >> a.m_follow_playhead >> a.m_input_state);
}

std::vector<Interval> Input::State::On::clock_ints(
    int end_clock, const pxtnMaster *master) const {
  if (end_clock == start_clock) return {};

  std::vector<Interval> clock_ints;
  int start_wrap = MasterExtended::wrapClock(master, start_clock);
  int end_wrap = MasterExtended::wrapClock(master, end_clock);

  int min_wrap = std::min(start_wrap, end_wrap);
  int max_wrap = std::max(start_wrap, end_wrap);
  if ((start_clock < end_clock) ==
      (start_wrap < end_wrap))  // This condition found by trial and error
    clock_ints.push_back({min_wrap, max_wrap});
  else {
    int repeat = MasterExtended::repeat_clock(master);
    int last = MasterExtended::last_clock(master);
    if (repeat < min_wrap) clock_ints.push_back({repeat, min_wrap});
    if (max_wrap < last) clock_ints.push_back({max_wrap, last});
  }
  return clock_ints;
}
