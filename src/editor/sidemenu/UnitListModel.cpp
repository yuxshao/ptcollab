#include "UnitListModel.h"

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>

#include "IconHelper.h"

inline Qt::CheckState checked_of_bool(bool b) {
  return b ? Qt::Checked : Qt::Unchecked;
}

inline QFlags<QItemSelectionModel::SelectionFlag> selection_flag_of_bool(
    bool b) {
  return ((b ? QItemSelectionModel::Select : QItemSelectionModel::Deselect) |
          QItemSelectionModel::Rows);
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
    case UnitListColumn::Pinned:
      if (role == Qt::CheckStateRole)
        return checked_of_bool(m_client->editState().m_pinned_unit_ids.count(
                                   m_client->unitIdMap().noToId(index.row())) >
                               0);
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
    case UnitListColumn::Pinned:
      if (role == Qt::CheckStateRole) {
        int id = m_client->unitIdMap().noToId(index.row());
        m_client->changeEditState(
            [&](EditState &e) {
              if (value.toInt() == Qt::Checked)
                e.m_pinned_unit_ids.insert(id);
              else
                e.m_pinned_unit_ids.erase(id);
            },
            false);
        return true;
      }
      return false;
      break;
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
    case UnitListColumn::Pinned:
      f |= Qt::ItemIsUserCheckable;
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
        case UnitListColumn::Played:
        case UnitListColumn::Pinned:
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
        case UnitListColumn::Pinned:
          return getIcon("select");  // TODO update
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
        case UnitListColumn::Pinned:
          return tr("Pinned");
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
  QStyleOptionViewItem o = option;
  if (m_selection->currentIndex().row() == index.row()) {
    QColor c = o.palette.highlight().color();
    c.setAlphaF(0.5);
    painter->fillRect(option.rect, c);
    o.font.setBold(true);
  }

  QStyledItemDelegate::paint(painter, o, index);
}
bool UnitListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) {
  if (!index.isValid()) return false;
  switch (event->type()) {
    case QEvent::MouseButtonPress: {
      m_last_index = index;
      switch (UnitListColumn(index.column())) {
        case UnitListColumn::Visible:
        case UnitListColumn::Played:
        case UnitListColumn::Pinned: {
          bool state = qvariant_cast<Qt::CheckState>(
                           index.data(Qt::CheckStateRole)) == Qt::Checked;
          m_last_set_checked = !state;
          model->setData(index, checked_of_bool(m_last_set_checked),
                         Qt::CheckStateRole);
        } break;
        case UnitListColumn::Name:
          m_last_click_had_ctrl =
              ((QMouseEvent *)event)->modifiers() & Qt::ControlModifier;
          if (m_last_click_had_ctrl) {
            bool state = m_selection->isRowSelected(index.row());
            m_last_set_checked = !state;
            m_selection->select(index,
                                selection_flag_of_bool(m_last_set_checked));
          } else
            m_selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);

          break;
      }
      return true;
    }

    case QEvent::MouseMove: {
      std::set<int> rowsInBetweenMove;
      int row = index.row();
      while (row != m_last_index.row()) {
        rowsInBetweenMove.insert(row);
        row += (row > m_last_index.row() ? -1 : 1);
      }
      // 2021-09-19: We check again that LeftButton is down because this
      // move event tends to trigger in ptCollage style even when we aren't
      // currently pressing anything (we never receive a release event
      // either).
      if (QApplication::mouseButtons() & Qt::LeftButton)
        for (int row : rowsInBetweenMove) {
          const QModelIndex indexAtRow = m_last_index.siblingAtRow(row);
          switch (UnitListColumn(m_last_index.column())) {
            case UnitListColumn::Visible:
            case UnitListColumn::Played:
            case UnitListColumn::Pinned: {
              m_last_index = indexAtRow;
              model->setData(indexAtRow, checked_of_bool(m_last_set_checked),
                             Qt::CheckStateRole);
            } break;

            case UnitListColumn::Name: {
              if (m_last_click_had_ctrl) {
                m_last_index = indexAtRow;
                m_selection->select(indexAtRow,
                                    selection_flag_of_bool(m_last_set_checked));
              } else
                m_selection->setCurrentIndex(index,
                                             QItemSelectionModel::NoUpdate);
            } break;
          }
        }
      return true;
    }

    default:
      return false;
  }
  return QStyledItemDelegate::editorEvent(event, model, option, index);
};
