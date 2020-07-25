#include "UnitListModel.h"

#include <QEvent>
#include <QPainter>

inline Qt::CheckState checked_of_bool(bool b) {
  return b ? Qt::Checked : Qt::Unchecked;
}
inline Qt::CheckState flip_checked(Qt::CheckState c) {
  return (c == Qt::Checked ? Qt::Unchecked : Qt::Checked);
}
QVariant UnitListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();

  const pxtnUnit *unit = m_pxtn->Unit_Get(index.row());
  switch (UnitListColumn(index.column())) {
    case UnitListColumn::Visible:
      if (role == Qt::CheckStateRole)
        return checked_of_bool(unit->get_visible());
      break;
    case UnitListColumn::Played:
      if (role == Qt::CheckStateRole)
        return checked_of_bool(unit->get_played());
      break;
    case UnitListColumn::Select:
      if (role == Qt::CheckStateRole)
        return checked_of_bool(unit->get_operated());
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
    case UnitListColumn::Played:
      if (role == Qt::CheckStateRole) {
        unit->set_played((value.toInt() == Qt::Checked));
        return true;
      }
      return false;
    case UnitListColumn::Select:
      if (role == Qt::CheckStateRole) {
        unit->set_operated(value.toInt() == Qt::Checked);
        dataChanged(index.siblingAtColumn(0),
                    index.siblingAtColumn(columnCount() - 1));
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
    case UnitListColumn::Played:
    case UnitListColumn::Select:
      f |= Qt::ItemIsUserCheckable;
      f &= ~Qt::ItemIsSelectable;
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
      case UnitListColumn::Played:
        return "P";
      case UnitListColumn::Select:
        return "S";
      case UnitListColumn::Name:
        return "Name";
    }
  }
  return QVariant();
}

void UnitListDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const {
  // TODO: Draw a speaker & eye icon!
  /*if (!index.isValid()) return;
  switch (UnitListColumn(index.column())) {
    case UnitListColumn::Visible:
    case UnitListColumn::Played:
      if (index.data(Qt::CheckStateRole) == Qt::Checked) {
        painter->fillRect(option.rect, Qt::black);
        return;
      }
      break;
    case UnitListColumn::Name:
      break;
  }*/
  if (index.siblingAtColumn(int(UnitListColumn::Select))
          .data(Qt::CheckStateRole) == Qt::Checked) {
    // A bit jank for a light highlight
    QColor c = option.palette.highlight().color();
    c.setAlphaF(0.3);
    painter->fillRect(option.rect, c);
  }

  QStyledItemDelegate::paint(painter, option, index);
}
bool UnitListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) {
  if (!index.isValid()) return false;
  switch (UnitListColumn(index.column())) {
    case UnitListColumn::Visible:
    case UnitListColumn::Played:
    case UnitListColumn::Select:
      switch (event->type()) {
        case QEvent::MouseButtonPress: {
          Qt::CheckState state =
              qvariant_cast<Qt::CheckState>(index.data(Qt::CheckStateRole));
          m_last_index = index;
          m_last_set_checked = flip_checked(state);
          model->setData(index, m_last_set_checked, Qt::CheckStateRole);
          return true;
        }

        case QEvent::MouseMove: {
          const QModelIndex indexAtColumn =
              index.siblingAtColumn(m_last_index.column());
          if (indexAtColumn.row() != m_last_index.row()) {
            m_last_index = indexAtColumn;
            model->setData(indexAtColumn, m_last_set_checked,
                           Qt::CheckStateRole);
          }
        }

        default:
          break;
      }
      return false;
    case UnitListColumn::Name:
      break;
  }
  return QStyledItemDelegate::editorEvent(event, model, option, index);
};
