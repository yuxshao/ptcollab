#include "OverdriveEffectModel.h"

OverdriveEffectModel::OverdriveEffectModel(PxtoneClient *client,
                                           QObject *parent)
    : QAbstractTableModel(parent), m_client(client) {
  const PxtoneController *controller = m_client->controller();
  connect(controller, &PxtoneController::beginRefresh, this,
          &OverdriveEffectModel::beginResetModel);
  connect(controller, &PxtoneController::endRefresh, this,
          &OverdriveEffectModel::endResetModel);
  connect(controller, &PxtoneController::beginAddOverdrive, [this]() {
    int n = m_client->pxtn()->OverDrive_Num();
    beginInsertRows(QModelIndex(), n, n);
  });
  connect(controller, &PxtoneController::endAddOverdrive, this,
          &OverdriveEffectModel::endInsertRows);
  connect(controller, &PxtoneController::beginRemoveOverdrive,
          [this](int index) { beginRemoveRows(QModelIndex(), index, index); });
  connect(controller, &PxtoneController::endRemoveOverdrive, this,
          &OverdriveEffectModel::endRemoveRows);
  connect(controller, &PxtoneController::overdriveChanged,
          [this](int ovdrv_no) {
            emit dataChanged(index(ovdrv_no, 0),
                             index(ovdrv_no, int(OverdriveEffectColumn::MAX)));
          });
}

QVariant OverdriveEffectModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();
  const pxtnOverDrive *ovdrv = m_client->pxtn()->OverDrive_Get(index.row());
  switch (OverdriveEffectColumn(index.column())) {
    case OverdriveEffectColumn::Group:
      if (role == Qt::DisplayRole || role == Qt::EditRole)
        return ovdrv->get_group();
      break;
    case OverdriveEffectColumn::Cut:
      if (role == Qt::DisplayRole)
        return QString("%1%").arg(ovdrv->get_cut(), 0, 'f', 1);
      else if (role == Qt::EditRole)
        return QString("%1").arg(ovdrv->get_cut(), 0, 'f', 1);
      break;
    case OverdriveEffectColumn::Amp:
      if (role == Qt::DisplayRole)
        return QString("x%1").arg(ovdrv->get_amp(), 0, 'f', 2);
      else if (role == Qt::EditRole)
        return QString("%1").arg(ovdrv->get_amp(), 0, 'f', 2);
      break;
  }
  return QVariant();
}

bool OverdriveEffectModel::setData(const QModelIndex &index,
                                   const QVariant &value, int role) {
  if (!checkIndex(index) || role != Qt::EditRole) return false;
  bool ok;

  const pxtnOverDrive *ovdrv = m_client->pxtn()->OverDrive_Get(index.row());
  qreal cut = ovdrv->get_cut();
  qreal amp = ovdrv->get_amp();
  qint32 group = ovdrv->get_group();
  switch (OverdriveEffectColumn(index.column())) {
    case OverdriveEffectColumn::Group:
      group = value.toInt(&ok);
      break;
    case OverdriveEffectColumn::Cut:
      cut = value.toReal(&ok);
      break;
    case OverdriveEffectColumn::Amp:
      amp = value.toReal(&ok);
      break;
  }
  if (ok) m_client->sendAction(OverdriveEffect::Set{index.row(), cut, amp, group});
  return ok;
}

Qt::ItemFlags OverdriveEffectModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags f = QAbstractTableModel::flags(index);
  if (!checkIndex(index)) return f;
  switch (OverdriveEffectColumn(index.column())) {
    case OverdriveEffectColumn::Group:
      f |= Qt::ItemIsEditable;
      break;
    case OverdriveEffectColumn::Cut:
      f |= Qt::ItemIsEditable;
      break;
    case OverdriveEffectColumn::Amp:
      f |= Qt::ItemIsEditable;
      break;
  }
  return f;
}

QVariant OverdriveEffectModel::headerData(int section,
                                          Qt::Orientation orientation,
                                          int role) const {
  if (orientation == Qt::Horizontal) {
    if (role == Qt::DisplayRole) {
      switch (OverdriveEffectColumn(section)) {
        case OverdriveEffectColumn::Group:
          return tr("G",
                    "Group (abbreviated if necessary, shown in table header)");
        case OverdriveEffectColumn::Cut:
          return tr("Cut",
                    "Cut (abbreviated if necessary, shown in table header)");
        case OverdriveEffectColumn::Amp:
          return tr("Amp.",
                    "Amplification (abbreviated if necessary, shown in table "
                    "header)");
      }
    }
    if (role == Qt::ToolTipRole) {
      switch (OverdriveEffectColumn(section)) {
        case OverdriveEffectColumn::Group:
          return tr("Group", "Group (full word, shown in tooltip)");
        case OverdriveEffectColumn::Cut:
          return tr("Cut", "Cut (full word, shown in tooltip)");
        case OverdriveEffectColumn::Amp:
          return tr("Amplification",
                    "Amplification (full word, shown in tooltip)");
      }
    }
  }
  return QVariant();
}
