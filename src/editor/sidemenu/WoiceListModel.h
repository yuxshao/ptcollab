#ifndef WOICELISTMODEL_H
#define WOICELISTMODEL_H

#ifndef WoiceLISTMODEL_H
#define WoiceLISTMODEL_H
#include <QAbstractTableModel>
#include <QStyledItemDelegate>

#include "editor/PxtoneClient.h"
#include "protocol/NoIdMap.h"

enum struct WoiceListColumn : qint8 { Loop, BeatFit, Key, Name, MAX = Name };

class WoiceListModel : public QAbstractTableModel {
  Q_OBJECT

  PxtoneClient *m_client;

 public:
  WoiceListModel(PxtoneClient *client, QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return m_client->pxtn()->Woice_Num();
  };
  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return int(WoiceListColumn::MAX) + 1;
  }
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
};

#endif  // WoiceLISTMODEL_H

#endif  // WOICELISTMODEL_H
