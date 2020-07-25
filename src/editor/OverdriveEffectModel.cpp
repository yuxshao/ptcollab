#include "OverdriveEffectModel.h"

OverdriveEffectModel::OverdriveEffectModel(PxtoneClient *client,
                                           QObject *parent)
    : QAbstractTableModel(parent), m_client(client) {
  const PxtoneController *controller = m_client->controller();
  connect(controller, &PxtoneController::beginRefresh, this,
          &OverdriveEffectModel::beginResetModel);
  connect(controller, &PxtoneController::endRefresh, this,
          &OverdriveEffectModel::endResetModel);
}

QVariant OverdriveEffectModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || role != Qt::DisplayRole) return QVariant();
  const pxtnOverDrive *Overdrive = m_client->pxtn()->OverDrive_Get(index.row());
  switch (OverdriveEffectColumn(index.column())) {
    case OverdriveEffectColumn::Group:
      return Overdrive->get_group();
    case OverdriveEffectColumn::Cut:
      return QString("%1%").arg(Overdrive->get_cut());
    case OverdriveEffectColumn::Amp:
      return QString("x%1").arg(Overdrive->get_amp());
  }
  return QVariant();
}

QVariant OverdriveEffectModel::headerData(int section,
                                          Qt::Orientation orientation,
                                          int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch (OverdriveEffectColumn(section)) {
      case OverdriveEffectColumn::Group:
        return "G";
      case OverdriveEffectColumn::Cut:
        return "Cut";
      case OverdriveEffectColumn::Amp:
        return "Amp.";
    }
  }
  return QVariant();
}
