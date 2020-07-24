#include "ViewHelper.h"

#include <QWheelEvent>

#include "editor/ComboOptions.h"
#include "pxtone/pxtnEvelist.h"

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
                               int height, qreal clockPerPx, bool drawHead) {
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

void drawRepeatAndEndBars(QPainter &painter, const MooClock *moo_clock,
                          qreal clockPerPx, int height) {
  painter.fillRect(moo_clock->last_clock() / clockPerPx, 0, 2, height,
                   Qt::white);
  painter.fillRect(moo_clock->last_clock() / clockPerPx + 2, 0, 3, height,
                   halfWhite);

  painter.fillRect(moo_clock->repeat_clock() / clockPerPx - 1, 0, 2, height,
                   Qt::white);
  painter.fillRect(moo_clock->repeat_clock() / clockPerPx - 4, 0, 3, height,
                   halfWhite);
}

void handleWheelEventWithModifier(QWheelEvent *event, PxtoneClient *client,
                                  bool scaleY) {
  qreal delta = event->angleDelta().y();
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->modifiers() & Qt::ShiftModifier) {
      // scale X
      client->changeEditState([&](EditState &e) {
        e.scale.clockPerPx *= pow(2, delta / 240.0);
        if (e.scale.clockPerPx < 0.5) e.scale.clockPerPx = 0.5;
        if (e.scale.clockPerPx > 128) e.scale.clockPerPx = 128;
      });
    } else if (scaleY) {
      // scale Y
      client->changeEditState([&](EditState &e) {
        e.scale.pitchPerPx *= pow(2, delta / 240.0);
        if (e.scale.pitchPerPx < 8) e.scale.pitchPerPx = 8;
        if (e.scale.pitchPerPx > PITCH_PER_KEY / 4)
          e.scale.pitchPerPx = PITCH_PER_KEY / 4;
      });
    }

    event->accept();
  } else if (event->modifiers() & Qt::AltModifier) {
    // In this case, alt flips the scroll direction.
    // Maybe alt shift could handle quantize y?
    delta = event->angleDelta().x();
    int size = sizeof(quantizeXOptions) / sizeof(quantizeXOptions[0]);
    client->changeEditState([&](EditState &e) {
      auto &qx = e.m_quantize_clock_idx;
      if (delta < 0 && qx < size - 1) qx++;
      if (delta > 0 && qx > 0) qx--;
    });
    event->accept();
  }
}

int lerp(double r, int a, int b) {
  if (r > 1) r = 1;
  if (r < 0) r = 0;
  return a + r * (b - a);
}

const Brush brushes[] = {
    0.0 / 7, 3.0 / 7, 6.0 / 7, 2.0 / 7, 5.0 / 7, 1.0 / 7, 4.0 / 7,
};
const int NUM_BRUSHES = sizeof(brushes) / sizeof(Brush);

int nonnegative_modulo(int x, int m) {
  if (m == 0) return 0;
  return ((x % m) + m) % m;
}
