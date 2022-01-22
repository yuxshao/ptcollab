#ifndef CONTROLLABLESPLITTER_H
#define CONTROLLABLESPLITTER_H

#include <QSplitter>

class ControllableSplitter : public QSplitter {
 public:
  ControllableSplitter(Qt::Orientation orientation, QWidget* parent = nullptr)
      : QSplitter(orientation, parent){};
  void moveSplitter(int pos, int index);
};

#endif  // CONTROLLABLESPLITTER_H
