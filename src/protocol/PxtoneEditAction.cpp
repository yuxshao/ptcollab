#include "PxtoneEditAction.h"

#include <QDebug>

namespace Action {

void perform(const Primitive &a, pxtnService *pxtn, bool *widthChanged,
             const UnitIdMap &map) {
  // if (a.kind == EVENTKIND_KEY) qDebug() << "Perform" << a;
  auto unit_no_maybe = map.idToNo(a.unit_id);
  if (unit_no_maybe == std::nullopt) return;
  qint32 unit_no = unit_no_maybe.value();
  std::visit(overloaded{[&](const Add &b) {
                          pxtn->evels->Record_Add_i(a.start_clock, unit_no,
                                                    a.kind, b.value);

                          // -1 since end is exclusive
                          int end_clock = a.start_clock;
                          if (Evelist_Kind_IsTail(a.kind))
                            end_clock += b.value - 1;

                          int clockPerMeas = pxtn->master->get_beat_clock() *
                                             pxtn->master->get_beat_num();
                          int end_meas = end_clock / clockPerMeas;
                          if (end_meas >= pxtn->master->get_meas_num()) {
                            if (widthChanged) *widthChanged = true;
                            pxtn->master->set_meas_num(end_meas + 1);
                            qDebug() << end_meas;
                          }
                        },
                        [&](const Delete &b) {
                          pxtn->evels->Record_Delete(a.start_clock, b.end_clock,
                                                     unit_no, a.kind);
                        },
                        [&](const Shift &b) {
                          pxtn->evels->Record_Value_Change(a.start_clock,
                                                           b.end_clock, unit_no,
                                                           a.kind, b.offset);
                        }},
             a.type);
}

std::list<Primitive> get_undo(const Primitive &a, const pxtnService *pxtn,
                              const UnitIdMap &map) {
  std::list<Primitive> undo;
  auto unit_no_maybe = map.idToNo(a.unit_id);
  if (unit_no_maybe == std::nullopt) return undo;
  qint32 unit_no = unit_no_maybe.value();

  std::visit(
      overloaded{
          [&](const Add &b) {
            int end_clock =
                Evelist_Kind_IsTail(a.kind)
                    ? a.start_clock + b.value
                    : a.start_clock + 1;  // +1 since end-time is exclusive
            undo.push_back(
                {a.kind, a.unit_id, a.start_clock, Delete{end_clock}});
          },
          [&](const Delete &b) {
            // find everything in this range and add actions to add them in.
            if (Evelist_Kind_IsTail(a.kind)) {
              const EVERECORD *p = pxtn->evels->get_Records();
              for (; p && p->clock < a.start_clock; p = p->next) {
                if (a.kind == p->kind && unit_no == p->unit_no)
                  if (p->clock + p->value > a.start_clock) {
                    // > instead of >= b/c exclusive. This was causing undos to
                    // blow up in size because it'd lead to a ton of empty noop
                    // actions that replace a note with the same.
                    undo.push_back(
                        {a.kind, a.unit_id, p->clock, Delete{a.start_clock}});
                    undo.push_back(
                        {a.kind, a.unit_id, p->clock, Add{p->value}});
                  }
              }
              for (; p && p->clock < b.end_clock; p = p->next)
                if (a.kind == p->kind && unit_no == p->unit_no)
                  undo.push_back({a.kind, a.unit_id, p->clock, Add{p->value}});
            } else {
              const EVERECORD *p = pxtn->evels->get_Records();
              for (; p && p->clock < a.start_clock; p = p->next) continue;
              for (; p && p->clock < b.end_clock; p = p->next)
                if (a.kind == p->kind && unit_no == p->unit_no)
                  undo.push_back({a.kind, a.unit_id, p->clock, Add{p->value}});
            }
          },
          [&](const Shift &b) {
            // This undo isn't quite right if we hit a limit in the first
            // direction. But it shouldn't desync, I think, because it's still
            // semi-roundtrippable (f -> f^-1 -> f = f).
            //
            // But still undoing a transpose is a bit unintuitive b/c other
            // people's edits will not be transposed backwards with your undo.
            undo.push_back({a.kind, a.unit_id, a.start_clock,
                            Shift{b.end_clock, -b.offset}});
          }},
      a.type);

  // for (const auto &a : undo) {
  //  if (a.kind == EVENTKIND_KEY) qDebug() << "Compute undo" << a;
  //}
  return undo;
}

std::list<Primitive> apply_and_get_undo(const std::list<Primitive> &actions,
                                        pxtnService *pxtn, bool *widthChanged,
                                        const UnitIdMap &map) {
  std::list<Primitive> undo;
  for (const Primitive &a : actions) {
    undo.splice(undo.begin(), get_undo(a, pxtn, map));
    perform(a, pxtn, widthChanged, map);
  }
  return undo;
}
QDataStream &operator<<(QDataStream &out, const Add &a) {
  return (out << a.value);
}
QDataStream &operator>>(QDataStream &in, Add &a) { return (in >> a.value); }
QDataStream &operator<<(QDataStream &out, const Delete &a) {
  return (out << a.end_clock);
}
QDataStream &operator>>(QDataStream &in, Delete &a) {
  return (in >> a.end_clock);
}
QDataStream &operator<<(QDataStream &out, const Shift &a) {
  return (out << a.offset << a.end_clock);
}
QDataStream &operator>>(QDataStream &in, Shift &a) {
  return (in >> a.offset >> a.end_clock);
}

QDataStream &operator<<(QDataStream &out, const Primitive &a) {
  out << qint8(a.kind) << a.unit_id << a.start_clock << a.type;
  return out;
}
QDataStream &operator>>(QDataStream &in, Primitive &a) {
  read_as_qint8(in, a.kind);
  in >> a.unit_id >> a.start_clock >> a.type;
  return in;
}
}  // namespace Action
