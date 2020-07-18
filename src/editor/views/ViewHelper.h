#ifndef VIEWHELPER_H
#define VIEWHELPER_H

#include <QColor>
#include <QPainter>
#include <QPainterPath>

extern void drawCursor(const QPoint &position, QPainter &painter,
                       const QColor &color, const QString &username,
                       qint64 uid);

#endif  // VIEWHELPER_H
