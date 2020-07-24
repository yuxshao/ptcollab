#ifndef OVERDRIVEEFFECTMODEL_H
#define OVERDRIVEEFFECTMODEL_H

#include <QAbstractTableModel>
#include <QStyledItemDelegate>

#include "pxtone/pxtnService.h"

enum struct OverdriveEffectColumn { Group, Cut, Amp, MAX = Amp };

class OverdriveEffectModel : public QAbstractTableModel {
  Q_OBJECT

  pxtnService *m_pxtn;

 public:
  void beginRefresh() { beginResetModel(); }
  void endRefresh() { endResetModel(); }
  OverdriveEffectModel(pxtnService *pxtn, QObject *parent = nullptr)
      : QAbstractTableModel(parent), m_pxtn(pxtn){};

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return m_pxtn->OverDrive_Num();
  };
  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return int(OverdriveEffectColumn::MAX) + 1;
  }
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  // bool setData(const QModelIndex &index, const QVariant &value,
  //             int role = Qt::EditRole) override;
  // Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
};

#endif  // OVERDRIVEEFFECTMODEL_H
