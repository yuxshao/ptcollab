#include "DelayEffectModel.h"

QVariant DelayEffectModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || role != Qt::DisplayRole) return QVariant();
  const pxtnDelay *delay = m_pxtn->Delay_Get(index.row());
  switch (DelayEffectColumn(index.column())) {
    case DelayEffectColumn::Group:
      return delay->get_group();
    case DelayEffectColumn::Unit:
      return QString(DELAYUNIT_names[delay->get_unit()]);
    case DelayEffectColumn::Frequency:
      return QString("%1 Hz").arg(delay->get_freq());
    case DelayEffectColumn::Ratio:
      return QString("%1%").arg(delay->get_rate());
  }
  return QVariant();
}

QVariant DelayEffectModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch (DelayEffectColumn(section)) {
      case DelayEffectColumn::Group:
        return "G";
      case DelayEffectColumn::Unit:
        return "Unit";
      case DelayEffectColumn::Frequency:
        return "Freq.";
      case DelayEffectColumn::Ratio:
        return "Ratio";
    }
  }
  return QVariant();
}
