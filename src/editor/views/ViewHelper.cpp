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

void drawCurrentPlayerPosition(QPainter &painter, MooClock *moo_clock,
                               int height, int clockPerPx, bool drawHead) {
  QColor color = (moo_clock->this_seek_caught_up() ? Qt::white : halfWhite);
  const int x = moo_clock->now() / clockPerPx;
  int s = 0;
  if (drawHead) {
    s = 4;
    QPainterPath path;
    path.moveTo(x - s, 0);
    path.lineTo(x + s, 0);
    path.lineTo(x, s);
    path.closeSubpath();
    painter.fillPath(path, color);
  }
  painter.fillRect(x, s, 1, height, color);
}
