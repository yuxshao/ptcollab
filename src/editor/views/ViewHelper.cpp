#include "ViewHelper.h"

void drawCursor(const QPoint &position, QPainter &painter, const QColor &color,
                const QString &username, qint64 uid) {
  QPainterPath path;
  path.moveTo(position);
  path.lineTo(position + QPoint(8, 0));
  path.lineTo(position + QPoint(0, 8));
  path.closeSubpath();
  painter.fillPath(path, color);
  painter.setPen(color);
  painter.setFont(QFont("Sans serif", 6));
  painter.drawText(position + QPoint(8, 13),
                   QString("%1 (%2)").arg(username).arg(uid));
}

QColor halfWhite(QColor::fromRgb(255, 255, 255, 128));
QColor slightTint(QColor::fromRgb(255, 255, 255, 32));
