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

 public:
  Clipboard(QObject *parent = nullptr);
  void copy(const std::set<int> &unit_nos, const Interval &range,
            const pxtnService *pxtn, const NoIdMap &woiceIdMap);
  PasteResult makePaste(const std::set<int> &unit_nos, qint32 start_clock,
                        const NoIdMap &unitIdMap);
  std::list<Action::Primitive> makeClear(const std::set<int> &unit_nos,
                                         const Interval &range,
                                         const NoIdMap &unitIdMap);

  // Shifting L/R is in the clipboard right now since it knows which event kinds
  // to move + the shfit is basically a copy / paste atm.
  PasteResult makeShift(const std::set<int> &unit_nos, const Interval &range,
                        qint32 dest_start_clock, const pxtnService *pxtn,
                        const NoIdMap &woiceIdMap, const NoIdMap &unitIdMap);
  void setKindIsCopied(EVENTKIND kind, bool set);
  bool kindIsCopied(EVENTKIND kind) const;
 signals:
  void copyKindsSet();
};

#endif  // CLIPBOARD_H
