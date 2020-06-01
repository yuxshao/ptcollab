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

PxtoneEditAction::PxtoneEditAction(std::vector<Action> &&actions,
                                   pxtnEvelist *evels)
    : m_redo(actions), m_is_done(false), m_evels(evels) {
  for (auto a = m_redo.rbegin(); a != m_redo.rend(); ++a) {
    switch (a->type) {
      case Action::ADD: {
        int end_clock = Evelist_Kind_IsTail(a->kind)
                            ? a->start_clock + a->end_clock_or_value
                            : a->start_clock;
        m_undo.push_back(
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
                m_undo.push_back({Action::DELETE, a->kind, a->unit_no, p->clock,
                                  a->start_clock});
                m_undo.push_back(
                    {Action::ADD, a->kind, a->unit_no, p->clock, p->value});
              }
          }
          for (; p && p->clock < a->end_clock_or_value; p = p->next)
            if (a->kind == p->kind && a->unit_no == p->unit_no)
              m_undo.push_back(
                  {Action::ADD, a->kind, a->unit_no, p->clock, p->value});
        } else {
          const EVERECORD *p = evels->get_Records();
          for (; p && p->clock < a->start_clock; p = p->next) continue;
          for (; p && p->clock < a->end_clock_or_value; p = p->next)
            if (a->kind == p->kind && a->unit_no == p->unit_no)
              m_undo.push_back(
                  {Action::ADD, a->kind, a->unit_no, p->clock, p->value});
        }
        break;
    }
  }
}

bool PxtoneEditAction::is_done() const { return m_is_done; }

void PxtoneEditAction::toggle() {
  const auto &actions = (is_done() ? m_undo : m_redo);
  for (const Action &a : actions) a.perform(m_evels);
  m_is_done = !m_is_done;
}
