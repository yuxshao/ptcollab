#include "DelayEffectModel.h"

DelayEffectModel::DelayEffectModel(PxtoneClient *client, QObject *parent)
    : QAbstractTableModel(parent), m_client(client) {
  const PxtoneController *controller = m_client->controller();
  connect(controller, &PxtoneController::beginRefresh, this,
          &DelayEffectModel::beginResetModel);
  connect(controller, &PxtoneController::endRefresh, this,
          &DelayEffectModel::endResetModel);
  connect(controller, &PxtoneController::delayChanged, [this](int delay_no) {
    emit dataChanged(index(delay_no, 0),
                     index(delay_no, int(DelayEffectColumn::MAX)));
  });
}

QVariant DelayEffectModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();
  const pxtnDelay *delay = m_client->pxtn()->Delay_Get(index.row());
  switch (DelayEffectColumn(index.column())) {
    case DelayEffectColumn::Group:
      if (role == Qt::EditRole || role == Qt::DisplayRole)
        return delay->get_group();
      break;
    case DelayEffectColumn::Unit:
      if (role == Qt::EditRole || role == Qt::DisplayRole)
        return QString(DELAYUNIT_name(delay->get_unit()));
      break;
    case DelayEffectColumn::Frequency:
      if (role == Qt::DisplayRole)
        return QString("%1 Hz").arg(delay->get_freq(), 0, 'f', 2);
      else if (role == Qt::EditRole)
        return QString("%1").arg(delay->get_freq(), 0, 'f', 2);
      break;
    case DelayEffectColumn::Rate:
      if (role == Qt::DisplayRole)
        return QString("%1%").arg(delay->get_rate(), 0, 'f', 1);
      else if (role == Qt::EditRole)
        return QString("%1").arg(delay->get_rate(), 0, 'f', 1);
  }
  return QVariant();
}

DELAYUNIT DELAYUNIT_fromQString(const QString &s, bool *ok) {
  *ok = true;
  char firstChar = s.front().toLower().toLatin1();
  // Needt to cast to char * to work for mac.
  if (firstChar == 's' || s == QString::fromUtf8((char *)(u8"秒")))
    return DELAYUNIT_Second;
  else if (firstChar == 'b' || s == QString::fromUtf8((char *)(u8"拍")))
    return DELAYUNIT_Beat;
  else if (firstChar == 'm' || s == QString::fromUtf8((char *)(u8"小節")) ||
           s == QString::fromUtf8((char *)(u8"小")) ||
           s == QString::fromUtf8((char *)(u8"節")))
    return DELAYUNIT_Meas;

  *ok = false;
  return DELAYUNIT_Meas;
}

bool DelayEffectModel::setData(const QModelIndex &index, const QVariant &value,
                               int role) {
  if (!checkIndex(index) || role != Qt::EditRole) return false;
  bool ok;

  const pxtnDelay *delay = m_client->pxtn()->Delay_Get(index.row());
  DelayEffect::Set action{index.row(), delay->get_unit(), delay->get_freq(),
                          delay->get_rate(), delay->get_group()};
  switch (DelayEffectColumn(index.column())) {
    case DelayEffectColumn::Group:
      action.group = value.toInt(&ok);
      break;
    case DelayEffectColumn::Unit:
      action.unit = DELAYUNIT_fromQString(value.toString(), &ok);
      break;
    case DelayEffectColumn::Frequency:
      action.freq = value.toReal(&ok);
      break;
    case DelayEffectColumn::Rate:
      action.rate = value.toReal(&ok);
      break;
  }
  qDebug() << "ACTION" << action.unit << action.freq << action.rate
           << action.group;
  if (ok) m_client->sendAction(action);
  return ok;
}

Qt::ItemFlags DelayEffectModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags f = QAbstractTableModel::flags(index);
  if (!checkIndex(index)) return f;
  switch (DelayEffectColumn(index.column())) {
    case DelayEffectColumn::Group:
      f |= Qt::ItemIsEditable;
      break;
    case DelayEffectColumn::Unit:
      f |= Qt::ItemIsEditable;
      break;
    case DelayEffectColumn::Frequency:
      f |= Qt::ItemIsEditable;
      break;
    case DelayEffectColumn::Rate:
      f |= Qt::ItemIsEditable;
      break;
  }
  return f;
}

QVariant DelayEffectModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const {
  if (orientation == Qt::Horizontal) {
    if (role == Qt::DisplayRole) {
      switch (DelayEffectColumn(section)) {
        case DelayEffectColumn::Group:
          return tr("G",
                    "Group (abbreviated if necessary, shown in table header)");
        case DelayEffectColumn::Unit:
          return tr("Unit",
                    "Unit (abbreviated if necessary, shown in table header)");
        case DelayEffectColumn::Frequency:
          return tr(
              "Freq.",
              "Frequency (abbreviated if necessary, shown in table header)");
        case DelayEffectColumn::Rate:
          return tr("Ratio",
                    "Ratio (abbreviated if necessary, shown in table header)");
      }
    }
    if (role == Qt::ToolTipRole) {
      switch (DelayEffectColumn(section)) {
        case DelayEffectColumn::Group:
          return tr("Group", "Group (full word, shown in tooltip)");
        case DelayEffectColumn::Unit:
          return tr("Unit", "Unit (full word, shown in tooltip)");
        case DelayEffectColumn::Frequency:
          return tr("Frequency", "Frequency (full word, shown in tooltip)");
        case DelayEffectColumn::Rate:
          return tr("Ratio", "Ratio (full word, shown in tooltip)");
      }
    }
  }
  return QVariant();
}
