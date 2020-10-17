#include "BasicWoiceListModel.h"

#include <QTextCodec>
static QTextCodec *codec = QTextCodec::codecForName("Shift-JIS");

BasicWoiceListModel::BasicWoiceListModel(PxtoneClient *client, QObject *parent)
    : QAbstractListModel(parent), m_client(client) {
  const PxtoneController *controller = m_client->controller();
  connect(controller, &PxtoneController::beginAddWoice, [this]() {
    int Woice_num = m_client->pxtn()->Woice_Num();
    beginInsertRows(QModelIndex(), Woice_num, Woice_num);
  });
  connect(controller, &PxtoneController::endAddWoice, this,
          &BasicWoiceListModel::endInsertRows);
  connect(controller, &PxtoneController::beginRemoveWoice,
          [this](int index) { beginRemoveRows(QModelIndex(), index, index); });
  connect(controller, &PxtoneController::endRemoveWoice, this,
          &BasicWoiceListModel::endRemoveRows);
  connect(controller, &PxtoneController::beginRefresh, this,
          &BasicWoiceListModel::beginResetModel);
  connect(controller, &PxtoneController::endRefresh, this,
          &BasicWoiceListModel::endResetModel);
  connect(controller, &PxtoneController::woiceEdited,
          [this](int no) { emit dataChanged(index(no, 0), index(no, 1)); });
}

QVariant BasicWoiceListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();

  std::shared_ptr<const pxtnWoice> woice =
      m_client->pxtn()->Woice_Get(index.row());

  if (role == Qt::DisplayRole)
    return codec->toUnicode(woice->get_name_buf_jis(nullptr));

  return QVariant();
}
