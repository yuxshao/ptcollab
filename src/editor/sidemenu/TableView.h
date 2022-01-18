#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <QObject>
#include <QTableView>
#include <optional>

class TableView : public QTableView {
  Q_OBJECT
  std::optional<int> m_hovered_row;
  void set_hovered_row(std::optional<int>);

 public:
  TableView(QWidget *parent = nullptr) : QTableView(parent){};
  void mouseMoveEvent(QMouseEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void updateRow(int);
 signals:
  void hoveredRowChanged(std::optional<int>);
};

#endif  // TABLEVIEW_H
