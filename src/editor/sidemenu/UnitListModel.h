#ifndef UNITLISTMODEL_H
#define UNITLISTMODEL_H
#include <QAbstractTableModel>
#include <QItemSelectionModel>
#include <QStyledItemDelegate>

#include "editor/PxtoneClient.h"
#include "protocol/NoIdMap.h"

enum struct UnitListColumn : qint8 { Visible, Played, Name, MAX = Name };

class UnitListModel : public QAbstractTableModel {
  Q_OBJECT

  PxtoneClient *m_client;

 public:
  UnitListModel(PxtoneClient *client, QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return m_client->pxtn()->Unit_Num();
  };
  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return int(UnitListColumn::MAX) + 1;
  }
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
};

class UnitListDelegate : public QStyledItemDelegate {
  Q_OBJECT
  QModelIndex m_last_index;
  QItemSelectionModel *m_selection;
  bool m_last_set_checked, m_last_click_had_ctrl;

 public:
  UnitListDelegate(QItemSelectionModel *selection)
      : m_selection(selection), m_last_click_had_ctrl(false){};
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  bool editorEvent(QEvent *event, QAbstractItemModel *model,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;
};

#endif  // UNITLISTMODEL_H
