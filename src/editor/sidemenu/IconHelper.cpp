#include "IconHelper.h"

#include <QApplication>
#include <QPalette>

QIcon getIcon(QString name) {
  if (QApplication::palette().color(QPalette::ButtonText) ==
      QColor(222, 217, 187))
    return QIcon(":/icons/tan/" + name);
  if (QApplication::palette().color(QPalette::Button).lightnessF() < 0.5)
    return QIcon(":/icons/dark/" + name);
  return QIcon(":/icons/light/" + name);
}
