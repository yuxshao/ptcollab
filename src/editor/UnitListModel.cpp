#include "UnitListModel.h"

QVariant UnitListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();

  const pxtnUnit *unit = m_pxtn->Unit_Get(index.row());
  switch (UnitListColumn(index.column())) {
    case UnitListColumn::Visible:
      if (role == Qt::CheckStateRole)
        return checked_of_bool(unit->get_visible());
      break;
    case UnitListColumn::Muted:
      if (role == Qt::CheckStateRole)
        return checked_of_bool(!unit->get_played());
      break;
    case UnitListColumn::Name:
      if (role == Qt::DisplayRole) return QString(unit->get_name_buf(nullptr));
      break;
  }
  return QVariant();
}

bool UnitListModel::setData(const QModelIndex &index, const QVariant &value,
                            int role) {
  if (!checkIndex(index)) return false;
  pxtnUnit *unit = m_pxtn->Unit_Get_variable(index.row());
  switch (UnitListColumn(index.column())) {
    case UnitListColumn::Visible:
      if (role == Qt::CheckStateRole) {
        unit->set_visible(value.toInt() == Qt::Checked);
        return true;
      }
      return false;
    case UnitListColumn::Muted:
      if (role == Qt::CheckStateRole) {
        unit->set_played(!(value.toInt() == Qt::Checked));
        return true;
      }
      return false;
    case UnitListColumn::Name:
      return false;
  }
  return false;
}

Qt::ItemFlags UnitListModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags f = QAbstractTableModel::flags(index);
  if (!checkIndex(index)) return f;
  switch (UnitListColumn(index.column())) {
    case UnitListColumn::Visible:
    case UnitListColumn::Muted:
      f |= Qt::ItemIsUserCheckable;
      break;
    case UnitListColumn::Name:
      break;
  }
  return f;
}

QVariant UnitListModel::headerData(int section, Qt::Orientation orientation,
                                   int role) const {
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
