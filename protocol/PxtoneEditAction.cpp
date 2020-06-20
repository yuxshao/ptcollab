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

std::vector<Action> apply_actions_and_get_undo(
    const std::vector<Action> &actions, pxtnService *pxtn, bool *widthChanged,
    const UnitIdMap &map) {
  std::vector<Action> undo;
  for (auto a = actions.rbegin(); a != actions.rend(); ++a) {
    auto unit_no_maybe = map.idToNo(a->unit_id);
    if (unit_no_maybe == std::nullopt) continue;
    qint32 unit_no = unit_no_maybe.value();

    switch (a->type) {
      case Action::ADD: {
        int end_clock =
            Evelist_Kind_IsTail(a->kind)
                ? a->start_clock + a->end_clock_or_value
                : a->start_clock + 1;  // +1 since end-time is exclusive
        undo.push_back(
            {Action::DELETE, a->kind, a->unit_id, a->start_clock, end_clock});
      } break;
      case Action::DELETE:
        // find everything in this range and add actions to add them in.
        if (Evelist_Kind_IsTail(a->kind)) {
          const EVERECORD *p = pxtn->evels->get_Records();
          for (; p && p->clock < a->start_clock; p = p->next) {
            if (a->kind == p->kind && unit_no == p->unit_no)
              if (p->clock + p->value >= a->start_clock) {
                // here the original action replaces a block with a smaller
                // block. so undo would replace the smaller block with the
                // original.
                undo.push_back({Action::DELETE, a->kind, a->unit_id, p->clock,
                                a->start_clock});
                undo.push_back(
                    {Action::ADD, a->kind, a->unit_id, p->clock, p->value});
              }
          }
          for (; p && p->clock < a->end_clock_or_value; p = p->next)
            if (a->kind == p->kind && unit_no == p->unit_no)
              undo.push_back(
                  {Action::ADD, a->kind, a->unit_id, p->clock, p->value});
        } else {
          const EVERECORD *p = pxtn->evels->get_Records();
          for (; p && p->clock < a->start_clock; p = p->next) continue;
          for (; p && p->clock < a->end_clock_or_value; p = p->next)
            if (a->kind == p->kind && unit_no == p->unit_no)
              undo.push_back(
                  {Action::ADD, a->kind, a->unit_id, p->clock, p->value});
        }
        break;
    }
  }
  for (const Action &a : actions) a.perform(pxtn, widthChanged, map);
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
