#ifndef CLIPBOARD_H
#define CLIPBOARD_H
#include <list>

#include "Interval.h"
#include "protocol/PxtoneEditAction.h"
#include "pxtone/pxtnEvelist.h"
#include "pxtone/pxtnService.h"

struct Item {
  int32_t clock;
  uint8_t unit_no;
  EVENTKIND kind;
  int32_t value;
};

class Clipboard {
  const pxtnService *m_pxtn;
  std::list<Item> m_items;
  std::set<int> m_unit_nos;
  qint32 m_copy_length;

 public:
  Clipboard(const pxtnService *pxtn);
  void copy(const std::set<int> &unit_nos, const Interval &range);
  std::list<Action::Primitive> makePaste(const std::set<int> &unit_nos,
                                         qint32 start_clock,
                                         const UnitIdMap &map);
};

#endif  // CLIPBOARD_H
