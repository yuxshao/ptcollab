#include "TableView.h"

#include <QMouseEvent>

void TableView::mouseMoveEvent(QMouseEvent *e) {
  QModelIndex index = indexAt(e->pos());
  std::optional<int> new_hovered_row = std::nullopt;
  if (index.isValid()) new_hovered_row = index.row();
  if (m_hovered_row != new_hovered_row) {
    m_hovered_row = new_hovered_row;
    emit hoveredRowChanged(new_hovered_row);
  }
  QTableView::mouseMoveEvent(e);
}
