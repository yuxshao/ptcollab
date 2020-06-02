#ifndef PXTONEEDITACTION_H
#define PXTONEEDITACTION_H

#include <set>
#include <vector>

#include "pxtone/pxtnEvelist.h"
struct Action {
  enum Type { ADD, DELETE };  // Add implicitly means add to an empty space
  Type type;
  EVENTKIND kind;
  int unit_no;
  int start_clock;
  int end_clock_or_value;  // depending on type
  void perform(pxtnEvelist *evels) const;
  void print() const;
};

class PxtoneEditAction {
 public:
  PxtoneEditAction(std::vector<Action> &&actions, pxtnEvelist *evels);

  void toggle();
  bool is_done() const;
  // bool intersects(PxtoneEditAction const &other) const;

 private:
  std::vector<Action> m_redo;
  std::vector<Action> m_undo;
  bool m_is_done;
  pxtnEvelist *m_evels;
  //  std::set<int> unit_range;
  //  int start_clock_range;
  //  int end_clock_range;
};

#endif  // PXTONEEDITACTION_H
