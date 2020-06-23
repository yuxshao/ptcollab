#include "PxtoneEditAction.h"

#include <QDebug>

void Action::print() const {
  std::string k;
  switch (kind) {
    case EVENTKIND_ON:
      k = "ON";
      break;
    case EVENTKIND_KEY:
      k = "KEY";
      break;
    case EVENTKIND_VELOCITY:
      k = "VEL";
      break;
    default:
      k = std::to_string(kind);
  }

  qDebug() << "ACTION(" << (type == Action::ADD ? "ADD" : "DELETE") << k.c_str()
           << unit_id << start_clock << end_clock_or_value << ")";
}

void Action::perform(pxtnService *pxtn, bool *widthChanged,
                     const UnitIdMap &map) const {
  auto unit_no_maybe = map.idToNo(unit_id);
  if (unit_no_maybe == std::nullopt) return;
  qint32 unit_no = unit_no_maybe.value();
  switch (type) {
    case Action::ADD:
      pxtn->evels->Record_Add_i(start_clock, unit_no, kind, end_clock_or_value);
      // Possibly extend the last measure

      {
        int end_clock = start_clock;
        // end_clock_or_value is value here b/c we're adding. -1 b/c exclusive
        if (Evelist_Kind_IsTail(kind)) end_clock += end_clock_or_value - 1;
        int clockPerMeas =
            pxtn->master->get_beat_clock() * pxtn->master->get_beat_num();
        int end_meas = end_clock / clockPerMeas;
        if (end_meas >= pxtn->master->get_meas_num()) {
          if (widthChanged) *widthChanged = true;
          pxtn->master->set_meas_num(end_meas + 1);
          qDebug() << end_meas;
        }
      }
      break;
    case Action::DELETE:
      pxtn->evels->Record_Delete(start_clock, end_clock_or_value, unit_no,
                                 kind);
      break;
  }
}

std::list<Action> Action::get_undo(const pxtnService *pxtn,
                                   const UnitIdMap &map) const {
  std::list<Action> undo;
  auto unit_no_maybe = map.idToNo(unit_id);
  if (unit_no_maybe == std::nullopt) return undo;
  qint32 unit_no = unit_no_maybe.value();

  switch (type) {
    case Action::ADD: {
      int end_clock = Evelist_Kind_IsTail(kind)
                          ? start_clock + end_clock_or_value
                          : start_clock + 1;  // +1 since end-time is exclusive
      undo.push_back({Action::DELETE, kind, unit_id, start_clock, end_clock});
    } break;
    case Action::DELETE:
      // find everything in this range and add actions to add them in.
      if (Evelist_Kind_IsTail(kind)) {
        const EVERECORD *p = pxtn->evels->get_Records();
        for (; p && p->clock < start_clock; p = p->next) {
          if (kind == p->kind && unit_no == p->unit_no)
            // > since end time is exclusive
            if (p->clock + p->value > start_clock) {
              // > instead of >= b/c exclusive. This was causing undos to blow
              // up in size because it'd lead to a ton of empty noop actions
              // that replace a note with the same.
              undo.push_back(
                  {Action::DELETE, kind, unit_id, p->clock, start_clock});
              undo.push_back({Action::ADD, kind, unit_id, p->clock, p->value});
            }
        }
        for (; p && p->clock < end_clock_or_value; p = p->next)
          if (kind == p->kind && unit_no == p->unit_no)
            undo.push_back({Action::ADD, kind, unit_id, p->clock, p->value});
      } else {
        const EVERECORD *p = pxtn->evels->get_Records();
        for (; p && p->clock < start_clock; p = p->next) continue;
        for (; p && p->clock < end_clock_or_value; p = p->next)
          if (kind == p->kind && unit_no == p->unit_no)
            undo.push_back({Action::ADD, kind, unit_id, p->clock, p->value});
      }
      break;
  }
  return undo;
}
std::list<Action> apply_actions_and_get_undo(const std::list<Action> &actions,
                                             pxtnService *pxtn,
                                             bool *widthChanged,
                                             const UnitIdMap &map) {
  std::list<Action> undo;
  // Apply each one at a time, getting their undo beforehand. I previously made
  // a mistake (that thankfully wasn't manifested) where I'd get all their undos
  // first, then apply. But this would be wrong if e.g., there were two
  // identical deletes (it'd lead to two identical additions even though the
  // second should do nothing)
  for (const Action &a : actions) {
    undo.splice(undo.begin(), a.get_undo(pxtn, map));
    a.perform(pxtn, widthChanged, map);
  }
  return undo;
}

QDataStream &operator<<(QDataStream &out, const Action &a) {
  out << qint8(a.type) << qint8(a.kind) << a.unit_id << a.start_clock
      << a.end_clock_or_value;
  return out;
}
QDataStream &operator>>(QDataStream &in, Action &a) {
  read_as_qint8(in, a.type);
  read_as_qint8(in, a.kind);
  in >> a.unit_id >> a.start_clock >> a.end_clock_or_value;
  return in;
}
