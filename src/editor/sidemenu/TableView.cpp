#include "TableView.h"

#include <QMouseEvent>

void TableView::set_hovered_row(std::optional<int> new_hovered_row) {
  if (m_hovered_row != new_hovered_row) {
    m_hovered_row = new_hovered_row;
    emit hoveredRowChanged(new_hovered_row);
  }
}

void TableView::mouseMoveEvent(QMouseEvent *e) {
  QModelIndex index = indexAt(e->pos());
  std::optional<int> new_hovered_row = std::nullopt;
  if (index.isValid()) new_hovered_row = index.row();
  set_hovered_row(new_hovered_row);
  QTableView::mouseMoveEvent(e);
}

void TableView::leaveEvent(QEvent *e) {
  set_hovered_row(std::nullopt);
  QTableView::leaveEvent(e);
}
