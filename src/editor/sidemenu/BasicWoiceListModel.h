#ifndef BASICWOICELISTMODEL_H
#define BASICWOICELISTMODEL_H

#include <QAbstractListModel>

#include "editor/PxtoneClient.h"

class BasicWoiceListModel : public QAbstractListModel {
  Q_OBJECT

  PxtoneClient *m_client;

 public:
  BasicWoiceListModel(PxtoneClient *client, QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return m_client->pxtn()->Woice_Num();
  };
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
};
#endif  // BASICWOICELISTMODEL_H
