#ifndef UNITLISTMODEL_H
#define UNITLISTMODEL_H

#include <QAbstractTableModel>

#include "protocol/UnitIdMap.h"
#include "pxtone/pxtnService.h"

enum struct UnitListColumn { Visible, Muted, Name, MAX = Name };

struct UnitListItem {
  bool visible;
  bool muted;
  QString name;
  qint32 id;  // TODO populate
  UnitListItem(QString name)
      : visible(false), muted(false), name(name), id(0){};
};

#include <QDebug>

inline Qt::CheckState checked_of_bool(bool b) {
  return b ? Qt::Checked : Qt::Unchecked;
}
class UnitListModel : public QAbstractTableModel {
  Q_OBJECT

  pxtnService *m_pxtn;

 public:
  // TODO: Perhaps we could change it so that indeed all unit edit ops go
  // through here. It's just... so leaky though. Technically it could go through
  // somewhere else.
  void beginAdd() {
    beginInsertRows(QModelIndex(), m_pxtn->Unit_Num(), m_pxtn->Unit_Num());
  }
  void endAdd() { endInsertRows(); };
  void beginRemove(int index) { beginRemoveRows(QModelIndex(), index, index); };
  void endRemove() { endRemoveRows(); }
  void beginRefresh() { beginResetModel(); }
  void endRefresh() { endResetModel(); }
  UnitListModel(pxtnService *pxtn, QObject *parent = nullptr)
      : QAbstractTableModel(parent), m_pxtn(pxtn){};

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return m_pxtn->Unit_Num();
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

#endif  // UNITLISTMODEL_H
