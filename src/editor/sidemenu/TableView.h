#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <QObject>
#include <QTableView>
#include <optional>

class TableView : public QTableView {
  Q_OBJECT
  std::optional<int> m_hovered_row;

 public:
  TableView(QWidget *parent = nullptr) : QTableView(parent){};
  void mouseMoveEvent(QMouseEvent *e) override;
 signals:
  void hoveredRowChanged(std::optional<int>);
};

#endif  // TABLEVIEW_H
