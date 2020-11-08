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

struct PasteResult {
  std::list<Action::Primitive> actions;
  qint32 length;
};

class Clipboard : public QObject {
  Q_OBJECT
  std::set<EVENTKIND> m_kinds_to_copy;
  friend QDataStream &operator<<(QDataStream &out, const Clipboard &a);
  friend QDataStream &operator>>(QDataStream &in, Clipboard &a);

 public:
  Clipboard(QObject *parent = nullptr);
  void copy(const std::set<int> &unit_nos, const Interval &range,
            const pxtnService *pxtn);
  PasteResult makePaste(const std::set<int> &unit_nos, qint32 start_clock,
                        const NoIdMap &map);
  std::list<Action::Primitive> makeClear(const std::set<int> &unit_nos,
                                         const Interval &range,
                                         const NoIdMap &map);
  void setKindIsCopied(EVENTKIND kind, bool set);
  bool kindIsCopied(EVENTKIND kind);
 signals:
  void copyKindsSet();
};

#endif  // CLIPBOARD_H
