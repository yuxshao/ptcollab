#include "UserListModel.h"

#include <QTimer>

UserListModel::UserListModel(PxtoneClient *client, QObject *parent,
                             int ping_refresh)
    : QAbstractTableModel(parent),
      m_client(client),
      m_ping_refresh_timer(new QTimer(this)) {
  connect(client, &PxtoneClient::beginAddUser,
          [this](int index) { beginInsertRows(QModelIndex(), index, index); });
  connect(client, &PxtoneClient::endAddUser, this,
          &UserListModel::endInsertRows);
  connect(client, &PxtoneClient::beginRemoveUser,
          [this](int index) { beginRemoveRows(QModelIndex(), index, index); });
  connect(client, &PxtoneClient::endRemoveUser, this,
          &UserListModel::endRemoveRows);
  connect(client, &PxtoneClient::beginUserListRefresh, this,
          &UserListModel::beginResetModel);
  connect(client, &PxtoneClient::endUserListRefresh, this,
          &UserListModel::endResetModel);

  connect(m_ping_refresh_timer, &QTimer::timeout, [this]() {
    if (rowCount() > 0) {
      emit dataChanged(index(int(UserListModel::Column::Ping), 0),
                       index(int(UserListModel::Column::Ping), rowCount() - 1));
    }
  });
  m_ping_refresh_timer->start(ping_refresh);
}
QVariant UserListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();

  if (role == Qt::DisplayRole) {
    auto user = m_client->remoteEditStates().begin();
    std::advance(user, index.row());

    switch (UserListModel::Column(index.column())) {
      case UserListModel::Column::Id:
        return user->first;
      case UserListModel::Column::Ping:
        if (user->second.last_ping.has_value()) {
          int ping = static_cast<int>(user->second.last_ping.value());
          if (ping > 1000) return QString("%1s").arg(ping / 1000);
          return ping + tr("ms");
        }
        return "";
      case UserListModel::Column::Name:
        return user->second.user;
    }
  }
  return QVariant();
}

QVariant UserListModel::headerData(int section, Qt::Orientation orientation,
                                   int role) const {
  if (orientation == Qt::Horizontal) {
    if (role == Qt::DisplayRole) {
      switch (UserListModel::Column(section)) {
        case UserListModel::Column::Id:
          return "#";
        case UserListModel::Column::Ping:
          return QVariant();
        case UserListModel::Column::Name:
          return tr("Name");
      }
    }
  }
  return QVariant();
}
