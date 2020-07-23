#ifndef VIEWHELPER_H
#define VIEWHELPER_H

#include <QColor>
#include <QPainter>
#include <QPainterPath>

#include "MooClock.h"

extern void drawCursor(const QPoint &position, QPainter &painter,
                       const QColor &color, const QString &username,
                       qint64 uid);
extern void drawCurrentPlayerPosition(QPainter &painter, MooClock *moo_clock,
                                      int height, int clockPerPx,
                                      bool drawHead);

extern QColor halfWhite, slightTint;

#endif  // VIEWHELPER_H
