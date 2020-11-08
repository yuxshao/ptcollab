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
  std::list<Item> m_items;
  std::set<int> m_unit_nos;
  // TODO: move this to editstate so you can see others' param selections too.
  std::set<EVENTKIND> m_kinds_to_copy;
  qint32 m_copy_length;
  friend QDataStream &operator<<(QDataStream &out, const Clipboard &a);
  friend QDataStream &operator>>(QDataStream &in, Clipboard &a);

 public:
  Clipboard(QObject *parent = nullptr);
  void copy(const std::set<int> &unit_nos, const Interval &range,
            const pxtnService *pxtn);
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
