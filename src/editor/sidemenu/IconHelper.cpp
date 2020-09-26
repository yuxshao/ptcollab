#include "IconHelper.h"

#include <QApplication>
#include <QPalette>

QIcon getIcon(QString name) {
  if (QApplication::palette().color(QPalette::Window).lightnessF() < 0.5)
    return QIcon(":/icons-dark/" + name);
  else
    return QIcon(":/icons/" + name);
}
