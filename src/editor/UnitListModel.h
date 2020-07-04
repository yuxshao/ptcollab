#ifndef UNITLISTMODEL_H
#define UNITLISTMODEL_H

#include <QAbstractTableModel>

enum struct UnitListColumn { Visible, Muted, Name, MAX = Name };

struct UnitListItem {
  bool visible;
  bool muted;
  QString name;
  qint32 id;  // TODO populate
  UnitListItem(QString name)
      : visible(false), muted(false), name(name), id(0){};
};

#include <QDebug>

class UnitListModel : public QAbstractTableModel {
  Q_OBJECT

  QList<UnitListItem> m_items;

 public:
  const QList<UnitListItem> items() { return m_items; };
  void setItem(int index, const UnitListItem &item) {
    m_items[index] = item;

    QModelIndex topLeft = createIndex(index, 0);
    QModelIndex bottomRight = createIndex(index, int(UnitListColumn::MAX));
    dataChanged(topLeft, bottomRight);
  }
  void add(const UnitListItem &item) {
    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    m_items.append(item);
    endInsertRows();
  };
  void remove(int index) {
    beginRemoveRows(QModelIndex(), index, index);
    m_items.removeAt(index);
    endRemoveRows();
  };
  void set(const QList<UnitListItem> &items) {
    beginResetModel();
    m_items = items;
    endResetModel();
  }
  UnitListModel(QObject *parent = nullptr) : QAbstractTableModel(parent){};

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return m_items.length();
  };
  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return int(UnitListColumn::MAX) + 1;
  }
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override {
    if (!index.isValid()) return QVariant();

    const UnitListItem &item = m_items.at(index.row());
    switch (UnitListColumn(index.column())) {
      case UnitListColumn::Visible:
        if (role == Qt::CheckStateRole) return item.visible;
        break;
      case UnitListColumn::Muted:
        if (role == Qt::CheckStateRole) return item.muted;
        break;
      case UnitListColumn::Name:
        if (role == Qt::DisplayRole) return item.name;
        break;
    }
    return QVariant();
  }
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override {
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
};

#endif  // UNITLISTMODEL_H
