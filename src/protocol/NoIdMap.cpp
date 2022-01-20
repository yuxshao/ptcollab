#include "NoIdMap.h"

#include <QDebug>

NoIdMap::NoIdMap(int start) : m_next_id(100) {
  for (int32_t i = 0; i < start; ++i) add();
}

std::optional<qint32> NoIdMap::idToNo(qint32 id) const {
  auto it = m_id_to_no.find(id);
  if (it == m_id_to_no.end()) return std::nullopt;
  return qint32(it->second);
}

void NoIdMap::add() {
  qint32 id = m_next_id++;
  size_t no = m_no_to_id.size();

  // equiv: m_no_to_id[no] = id;
  m_no_to_id.push_back(id);
  m_id_to_no[id] = no;

  // qDebug() << QVector<qint32>(m_no_to_id.begin(), m_no_to_id.end())
  //         << QMap<qint32, qint32>(m_id_to_no);
}

void NoIdMap::remove(size_t no) {
  qint32 id = m_no_to_id[no];
  m_no_to_id.erase(m_no_to_id.begin() + no);
  m_id_to_no.erase(id);
  for (auto &it : m_id_to_no)
    if (it.second > no) --it.second;

  // qDebug() << QVector<qint32>(m_no_to_id.begin(), m_no_to_id.end())
  //         << QMap<qint32, size_t>(m_id_to_no);
}

void NoIdMap::swapAdjacent(size_t no1, size_t no2) {
  if (no1 - no2 != 1 && no2 - no1 != 1) {
    qDebug() << "Not adjacent swap" << no1 << no2;
    return;
  }
  if (no1 >= m_no_to_id.size() || no2 >= m_no_to_id.size()) {
    qDebug() << "swapAdjacent value too big" << no1 << no2 << m_no_to_id.size();
    return;
  }
  std::swap(m_no_to_id[no1], m_no_to_id[no2]);
  m_id_to_no[m_no_to_id[no2]] = no2;
  m_id_to_no[m_no_to_id[no1]] = no1;
}
