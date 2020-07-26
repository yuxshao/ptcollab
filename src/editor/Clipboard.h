#ifndef CLIPBOARD_H
#define CLIPBOARD_H
#include <QObject>
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

class Clipboard : public QObject {
  Q_OBJECT
  const pxtnService *m_pxtn;
  std::list<Item> m_items;
  std::set<int> m_unit_nos;
  std::set<EVENTKIND> m_kinds_to_copy;
  qint32 m_copy_length;

 public:
  Clipboard(const pxtnService *pxtn, QObject *parent = nullptr);
  void copy(const std::set<int> &unit_nos, const Interval &range);
  std::list<Action::Primitive> makePaste(const std::set<int> &unit_nos,
                                         qint32 start_clock,
                                         const NoIdMap &map);
  std::list<Action::Primitive> makeClear(const std::set<int> &unit_nos,
                                         const Interval &range,
                                         const NoIdMap &map);
  void setKindIsCopied(EVENTKIND kind, bool set);
  bool kindIsCopied(EVENTKIND kind);

  qint32 copyLength() { return m_copy_length; }
 signals:
  void copyKindsSet();
};

#endif  // CLIPBOARD_H
