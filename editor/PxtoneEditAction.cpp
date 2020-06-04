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
           << unit_no << start_clock << end_clock_or_value << ")";
}
void Action::perform(pxtnEvelist *evels) const {
  switch (type) {
    case Action::ADD:
      evels->Record_Add_i(start_clock, unit_no, kind, end_clock_or_value);
      break;
    case Action::DELETE:
      evels->Record_Delete(start_clock, end_clock_or_value, unit_no, kind);
      break;
  }
}

std::vector<Action> apply_actions_and_get_undo(
    const std::vector<Action> &actions, pxtnEvelist *evels) {
  std::vector<Action> undo;
  for (auto a = actions.rbegin(); a != actions.rend(); ++a) {
    switch (a->type) {
      case Action::ADD: {
        int end_clock =
            Evelist_Kind_IsTail(a->kind)
                ? a->start_clock + a->end_clock_or_value
                : a->start_clock + 1;  // +1 since end-time is exclusive
        undo.push_back(
            {Action::DELETE, a->kind, a->unit_no, a->start_clock, end_clock});
      } break;
      case Action::DELETE:
        // find everything in this range and add actions to add them in.
        if (Evelist_Kind_IsTail(a->kind)) {
          const EVERECORD *p = evels->get_Records();
          for (; p && p->clock < a->start_clock; p = p->next) {
            if (a->kind == p->kind && a->unit_no == p->unit_no)
              if (p->clock + p->value >= a->start_clock) {
                // here the original action replaces a block with a smaller
                // block. so undo would replace the smaller block with the
                // original.
                undo.push_back({Action::DELETE, a->kind, a->unit_no, p->clock,
                                a->start_clock});
                undo.push_back(
                    {Action::ADD, a->kind, a->unit_no, p->clock, p->value});
              }
          }
          for (; p && p->clock < a->end_clock_or_value; p = p->next)
            if (a->kind == p->kind && a->unit_no == p->unit_no)
              undo.push_back(
                  {Action::ADD, a->kind, a->unit_no, p->clock, p->value});
        } else {
          const EVERECORD *p = evels->get_Records();
          for (; p && p->clock < a->start_clock; p = p->next) continue;
          for (; p && p->clock < a->end_clock_or_value; p = p->next)
            if (a->kind == p->kind && a->unit_no == p->unit_no)
              undo.push_back(
                  {Action::ADD, a->kind, a->unit_no, p->clock, p->value});
        }
        break;
    }
  }
  for (const Action &a : actions) a.perform(evels);
  return undo;
}
