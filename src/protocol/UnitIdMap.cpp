#include "UnitIdMap.h"

UnitIdMap::UnitIdMap(const pxtnService *pxtn) : m_next_id(0) {
  for (int32_t i = 0; i < pxtn->Unit_Num(); ++i) addUnit();
}

std::optional<qint32> UnitIdMap::idToNo(qint32 id) const {
  auto it = m_id_to_no.find(id);
  if (it == m_id_to_no.end()) return std::nullopt;
  return qint32(it->second);
}

void UnitIdMap::addUnit() {
  qint32 id = m_next_id++;
  size_t no = m_no_to_id.size();

  // equiv: m_no_to_id[no] = id;
  m_no_to_id.push_back(id);
  m_id_to_no[id] = no;

  // qDebug() << QVector<qint32>(m_no_to_id.begin(), m_no_to_id.end())
  //         << QMap<qint32, qint32>(m_id_to_no);
}

void UnitIdMap::removeUnit(size_t no) {
  qint32 id = m_no_to_id[no];
  m_no_to_id.erase(m_no_to_id.begin() + no);
  m_id_to_no.erase(id);
  for (auto &it : m_id_to_no)
    if (it.second > no) --it.second;

  // qDebug() << QVector<qint32>(m_no_to_id.begin(), m_no_to_id.end())
  //         << QMap<qint32, size_t>(m_id_to_no);
}
