#ifndef UNITIDMAP_H
#define UNITIDMAP_H

#include <QObject>
#include <map>
#include <optional>
#include <vector>

#include "pxtone/pxtnService.h"
// Because unit nos are fixed in pxtone, we have to maintain this mapping so
// that actions are still valid past unit additions / deletions / moves.
class UnitIdMap {
 public:
  UnitIdMap(const pxtnService *pxtn);
  qint32 numUnits() const { return m_no_to_id.size(); }
  qint32 noToId(qint32 no) const { return m_no_to_id[no]; };
  std::optional<qint32> idToNo(qint32 id) const;
  void addUnit();
  void removeUnit(size_t no);
  // TODO: move unit

 private:
  int m_next_id;
  std::map<qint32, size_t> m_id_to_no;
  std::vector<qint32> m_no_to_id;
};

#endif  // UNITIDMAP_H
