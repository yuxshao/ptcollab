#include "WoiceListModel.h"

#include <QEvent>
#include <QPainter>

#include "IconHelper.h"

inline Qt::CheckState checked_of_bool(bool b) {
  return b ? Qt::Checked : Qt::Unchecked;
}
inline Qt::CheckState flip_checked(Qt::CheckState c) {
  return (c == Qt::Checked ? Qt::Unchecked : Qt::Checked);
}

WoiceListModel::WoiceListModel(PxtoneClient *client, QObject *parent)
    : QAbstractTableModel(parent), m_client(client) {
  const PxtoneController *controller = m_client->controller();
  connect(controller, &PxtoneController::beginAddWoice, [this]() {
    int Woice_num = m_client->pxtn()->Woice_Num();
    beginInsertRows(QModelIndex(), Woice_num, Woice_num);
  });
  connect(controller, &PxtoneController::endAddWoice, this,
          &WoiceListModel::endInsertRows);
  connect(controller, &PxtoneController::beginRemoveWoice,
          [this](int index) { beginRemoveRows(QModelIndex(), index, index); });
  connect(controller, &PxtoneController::endRemoveWoice, this,
          &WoiceListModel::endRemoveRows);
  connect(controller, &PxtoneController::beginRefresh, this,
          &WoiceListModel::beginResetModel);
  connect(controller, &PxtoneController::endRefresh, this,
          &WoiceListModel::endResetModel);
  connect(controller, &PxtoneController::woiceEdited, [this](int no) {
    emit dataChanged(index(no, 0), index(no, int(WoiceListColumn::MAX)));
  });
}
QVariant WoiceListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();

  std::shared_ptr<const pxtnWoice> woice =
      m_client->pxtn()->Woice_Get(index.row());

  uint32_t voice_flags = 0;
  QString basic_key = 0;
  for (int i = 0; i < woice->get_voice_num(); ++i) {
    QString key =
        QString("%1").arg(woice->get_voice(i)->basic_key / PITCH_PER_KEY);
    if (i > 0 && key != basic_key)
      basic_key = "";
    else
      basic_key = key;
    voice_flags |= woice->get_voice(i)->voice_flags;
  }
  switch (WoiceListColumn(index.column())) {
    case WoiceListColumn::Loop:
      if (role == Qt::CheckStateRole)
        return checked_of_bool(voice_flags & PTV_VOICEFLAG_WAVELOOP);
      break;
    case WoiceListColumn::BeatFit:
      if (role == Qt::CheckStateRole)
        return checked_of_bool(voice_flags & PTV_VOICEFLAG_BEATFIT);
      break;
    case WoiceListColumn::Key:
      if (role == Qt::DisplayRole || role == Qt::EditRole) return basic_key;
      break;
    case WoiceListColumn::Name:
      if (role == Qt::DisplayRole || role == Qt::EditRole)
        return QString(woice->get_name_buf(nullptr));
      break;
  }
  return QVariant();
}

bool WoiceListModel::setData(const QModelIndex &index, const QVariant &value,
                             int role) {
  if (!checkIndex(index)) return false;
  std::shared_ptr<const pxtnWoice> woice =
      m_client->pxtn()->Woice_Get(index.row());

  // TODO: need woice id map
  Woice::Setting setting;
  bool ok;
  switch (WoiceListColumn(index.column())) {
    case WoiceListColumn::Loop:
      if (role == Qt::CheckStateRole) {
        setting =
            Woice::Flag{Woice::Flag::LOOP, value.toInt(&ok) == Qt::Checked};
        break;
      } else
        return false;
    case WoiceListColumn::BeatFit:
      if (role == Qt::CheckStateRole) {
        setting =
            Woice::Flag{Woice::Flag::BEATFIT, value.toInt(&ok) == Qt::Checked};
        break;
      } else
        return false;
    case WoiceListColumn::Key:
      if (role == Qt::EditRole) {
        setting = Woice::Key{value.toInt(&ok) * PITCH_PER_KEY};
        break;
      } else
        return false;
    case WoiceListColumn::Name:
      if (role == Qt::EditRole) {
        ok = true;
        setting = Woice::Name{value.toString()};
        break;
      } else
        return false;
  }
  if (ok) m_client->sendAction(Woice::Set{index.row(), setting});
  return ok;
}

Qt::ItemFlags WoiceListModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags f = QAbstractTableModel::flags(index);
  if (!checkIndex(index)) return f;
  std::shared_ptr<const pxtnWoice> woice =
      m_client->pxtn()->Woice_Get(index.row());
  switch (WoiceListColumn(index.column())) {
    case WoiceListColumn::Loop:
    case WoiceListColumn::BeatFit:
      f |= Qt::ItemIsUserCheckable;
      if (woice->get_type() == pxtnWOICE_PTV)
        f &= ~Qt::ItemIsEnabled & ~Qt::ItemIsUserCheckable;
      // f &= ~Qt::ItemIsSelectable;
      break;
    case WoiceListColumn::Key:
      f |= Qt::ItemIsEditable;
      // f &= ~Qt::ItemIsSelectable;
      break;
    case WoiceListColumn::Name:
      f |= Qt::ItemIsEditable;
      break;
  }
  return f;
}

QVariant WoiceListModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (orientation == Qt::Horizontal) {
    if (role == Qt::DisplayRole) {
      switch (WoiceListColumn(section)) {
        case WoiceListColumn::Loop:
        case WoiceListColumn::BeatFit:
          break;
        case WoiceListColumn::Key:
          return "Key";
        case WoiceListColumn::Name:
          return "Name";
      }
    }
    if (role == Qt::DecorationRole) {
      switch (WoiceListColumn(section)) {
        case WoiceListColumn::Loop:
          return getIcon("loop");
        case WoiceListColumn::BeatFit:
          return getIcon("measure");
        case WoiceListColumn::Key:
        case WoiceListColumn::Name:
          break;
      }
    }
    if (role == Qt::ToolTipRole) {
      switch (WoiceListColumn(section)) {
        case WoiceListColumn::Loop:
          return tr("Loop");
        case WoiceListColumn::BeatFit:
          return tr("Beat fit");
        case WoiceListColumn::Key:
          return tr("Key");
        case WoiceListColumn::Name:
          return tr("Name");
      }
    }
  }
  return QVariant();
}
