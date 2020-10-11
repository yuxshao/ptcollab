#ifndef USERLISTMODEL_H
#define USERLISTMODEL_H

#include <QAbstractTableModel>
#include <QStyledItemDelegate>

#include "editor/PxtoneClient.h"
#include "protocol/NoIdMap.h"

class UserListModel : public QAbstractTableModel {
  Q_OBJECT

  PxtoneClient *m_client;
  QTimer *m_ping_refresh_timer;

 public:
  enum struct Column : qint8 { Id, Name, Ping, MAX = Ping };
  UserListModel(PxtoneClient *client, QObject *parent = nullptr,
                int ping_refresh = 2000);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return m_client->remoteEditStates().size();
  };
  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return int(Column::MAX) + 1;
  }
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
};

#endif  // USERLISTMODEL_H
