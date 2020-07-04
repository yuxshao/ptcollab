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

class UnitListModel : public QAbstractTableModel {
  Q_OBJECT

  const pxtnService *m_pxtn;

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
                int role = Qt::DisplayRole) const override {
    if (!index.isValid()) return QVariant();

    const pxtnUnit *unit = m_pxtn->Unit_Get(index.row());
    switch (UnitListColumn(index.column())) {
      case UnitListColumn::Visible:
        if (role == Qt::CheckStateRole) return unit->get_visible();
        break;
      case UnitListColumn::Muted:
        if (role == Qt::CheckStateRole) return !(unit->get_played());
        break;
      case UnitListColumn::Name:
        if (role == Qt::DisplayRole)
          return QString(unit->get_name_buf(nullptr));
        break;
    }
    return QVariant();
  }
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override {
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
      switch (UnitListColumn(section)) {
        case UnitListColumn::Visible:
          return "V";
          break;
        case UnitListColumn::Muted:
          return "M";
          break;
        case UnitListColumn::Name:
          return "Name";
          break;
      }
    }
    return QVariant();
  }
};

#endif  // UNITLISTMODEL_H
