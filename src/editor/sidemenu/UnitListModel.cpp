#include "UnitListModel.h"

#include <QApplication>
#include <QEvent>
#include <QPainter>

#include "IconHelper.h"

inline Qt::CheckState checked_of_bool(bool b) {
  return b ? Qt::Checked : Qt::Unchecked;
}
inline Qt::CheckState flip_checked(Qt::CheckState c) {
  return (c == Qt::Checked ? Qt::Unchecked : Qt::Checked);
}

UnitListModel::UnitListModel(PxtoneClient *client, QObject *parent)
    : QAbstractTableModel(parent), m_client(client) {
  const PxtoneController *controller = m_client->controller();
  connect(controller, &PxtoneController::beginAddUnit, [this]() {
    int unit_num = m_client->pxtn()->Unit_Num();
    beginInsertRows(QModelIndex(), unit_num, unit_num);
  });
  connect(controller, &PxtoneController::endAddUnit, this,
          &UnitListModel::endInsertRows);
  connect(controller, &PxtoneController::beginRemoveUnit,
          [this](int index) { beginRemoveRows(QModelIndex(), index, index); });
  connect(controller, &PxtoneController::endRemoveUnit, this,
          &UnitListModel::endRemoveRows);
  connect(controller, &PxtoneController::beginRefresh, this,
          &UnitListModel::beginResetModel);
  connect(controller, &PxtoneController::endRefresh, this,
          &UnitListModel::endResetModel);
  connect(controller, &PxtoneController::beginMoveUnit,
          [this](int unit_no, bool up) {
            UnitListModel::beginMoveRows(QModelIndex(), unit_no, unit_no,
                                         QModelIndex(),
                                         (up ? unit_no - 1 : unit_no + 2));
          });
  connect(controller, &PxtoneController::endMoveUnit, this,
          &UnitListModel::endMoveRows);
  connect(controller, &PxtoneController::unitNameEdited, [this](int unit_no) {
    QModelIndex i = index(unit_no, int(UnitListColumn::Name));
    emit dataChanged(i, i);
  });
  connect(controller, &PxtoneController::playedToggled, [this](int unit_no) {
    QModelIndex i = index(unit_no, int(UnitListColumn::Played));
    emit dataChanged(i, i);
  });
  connect(controller, &PxtoneController::soloToggled, [this]() {
    int col = int(UnitListColumn::Played);
    emit dataChanged(index(0, col), index(rowCount() - 1, col));
  });
}
QVariant UnitListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();

  const pxtnUnit *unit = m_client->pxtn()->Unit_Get(index.row());
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
      if (role == Qt::DisplayRole || role == Qt::EditRole)
        return shift_jis_codec->toUnicode(unit->get_name_buf_jis(nullptr));
      break;
  }
  return QVariant();
}

bool UnitListModel::setData(const QModelIndex &index, const QVariant &value,
                            int role) {
  if (!checkIndex(index)) return false;
  switch (UnitListColumn(index.column())) {
    case UnitListColumn::Visible:
      if (role == Qt::CheckStateRole) {
        m_client->setUnitVisible(index.row(), value.toInt() == Qt::Checked);
        return true;
      }
      return false;
    case UnitListColumn::Played:
      if (role == Qt::CheckStateRole) {
        m_client->setUnitPlayed(index.row(), value.toInt() == Qt::Checked);
        return true;
      }
      return false;
    case UnitListColumn::Select:
      if (role == Qt::CheckStateRole) {
        m_client->setUnitOperated(index.row(), value.toInt() == Qt::Checked);
        dataChanged(index.siblingAtColumn(0),
                    index.siblingAtColumn(columnCount() - 1));
        return true;
      }
      return false;
    case UnitListColumn::Name:
      int unit_id = m_client->unitIdMap().noToId(index.row());
      m_client->sendAction(SetUnitName{unit_id, value.toString()});
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
      f |= Qt::ItemIsEditable;
      break;
  }
  return f;
}

QVariant UnitListModel::headerData(int section, Qt::Orientation orientation,
                                   int role) const {
  if (orientation == Qt::Horizontal) {
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
      switch (UnitListColumn(section)) {
        case UnitListColumn::Visible:
          break;
        case UnitListColumn::Played:
          break;
        case UnitListColumn::Select:
          break;
        case UnitListColumn::Name:
          return "Name";
      }
    }
    if (role == Qt::DecorationRole) {
      switch (UnitListColumn(section)) {
        case UnitListColumn::Visible:
          return getIcon("visible");
        case UnitListColumn::Played:
          return getIcon("audio-on");
        case UnitListColumn::Select:
          return getIcon("select");
        default:
          break;
      }
    }
    if (role == Qt::ToolTipRole) {
      switch (UnitListColumn(section)) {
        case UnitListColumn::Visible:
          return tr("Visible");
        case UnitListColumn::Played:
          return tr("Played");
        case UnitListColumn::Select:
          return tr("Selected");
        case UnitListColumn::Name:
          return tr("Name");
      }
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
