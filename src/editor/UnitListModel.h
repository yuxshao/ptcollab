#ifndef UNITLISTMODEL_H
#define UNITLISTMODEL_H

#include <QAbstractTableModel>

enum struct UnitListColumn { Visible, Muted, Name, MAX = Name };

struct UnitListItem {
  bool visible;
  bool muted;
  QString name;
  qint32 id;
};

class UnitList : public QObject {
  Q_OBJECT
  QList<UnitListItem> m_items;

 signals:
  void itemChanged(int index);
  void listChanged(int prev_size, int new_size);

 public:
  const QList<UnitListItem> items() { return m_items; };
  void setItem(int index, const UnitListItem &item) {
    m_items[index] = item;
    emit itemChanged(index);
  }
  void set(const QList<UnitListItem> &items) {
    int prev_size = m_items.length();
    m_items = items;
    emit listChanged(prev_size, m_items.length());
  }
};

class UnitListModel : public QAbstractTableModel {
  Q_OBJECT
  UnitList *m_units;

 public:
  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return m_units->items().length();
  };
  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    (void)parent;
    return int(UnitListColumn::MAX) + 1;
  };
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override {
    if (!index.isValid()) return QVariant();
    const UnitListItem &item = m_units->items().at(index.row());
    switch (UnitListColumn(index.column())) {
      case UnitListColumn::Visible:
        if (role == Qt::CheckStateRole) return item.visible;
      case UnitListColumn::Muted:
        if (role == Qt::CheckStateRole) return item.muted;
        break;
      case UnitListColumn::Name:
        if (role == Qt::DisplayRole) return item.name;
        break;
    }
    return QVariant();
  };
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
  UnitListModel(UnitList *units) : m_units(units) {
    connect(m_units, &UnitList::itemChanged, [this](int row) {
      QModelIndex topLeft = createIndex(row, 0);
      QModelIndex bottomRight = createIndex(row, int(UnitListColumn::MAX));
      dataChanged(topLeft, bottomRight);
    });
    connect(m_units, &UnitList::listChanged,
            [this](int prev_size, int new_size) {
              QModelIndex topLeft = createIndex(0, 0);
              QModelIndex bottomRight = createIndex(
                  std::max(prev_size, new_size), int(UnitListColumn::MAX));
              emit dataChanged(topLeft, bottomRight);
            });
  };
};

#endif  // UNITLISTMODEL_H
