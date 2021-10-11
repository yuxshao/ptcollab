#include "ViewHelper.h"

#include <QWheelEvent>

#include "editor/ComboOptions.h"
#include "editor/Settings.h"
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
  painter.setFont(QFont("Sans serif", Settings::TextSize::get()));
  painter.drawText(position + QPoint(8, 13),
                   QString("%1 (%2)").arg(username).arg(uid));
}

QColor halfWhite(QColor::fromRgb(255, 255, 255, 128));
QColor slightTint(QColor::fromRgb(255, 255, 255, 32));

void drawPlayhead(QPainter &painter, qint32 x, qint32 height, QColor color,
                  bool drawHead) {
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

void drawCurrentPlayerPosition(QPainter &painter, MooClock *moo_clock,
                               int height, qreal clockPerPx, bool drawHead) {
  QColor color =
      (moo_clock->this_seek_caught_up() && moo_clock->now() > 0 ? Qt::white
                                                                : halfWhite);
  const int x = moo_clock->now() / clockPerPx;
  drawPlayhead(painter, x, height, color, drawHead);
}

void drawLastSeek(QPainter &painter, const PxtoneClient *client, qint32 height,
                  bool drawHead) {
  if (client->following_uid() == client->uid())
    drawPlayhead(painter,
                 client->lastSeek() / client->editState().scale.clockPerPx,
                 height, QColor::fromRgb(255, 255, 255, 128), drawHead);
}

void drawRepeatAndEndBars(QPainter &painter, const MooClock *moo_clock,
                          qreal clockPerPx, int height) {
  if (moo_clock->has_last())
    painter.fillRect(moo_clock->last_clock() / clockPerPx, 0, 1, height,
                     halfWhite);

  painter.fillRect(moo_clock->repeat_clock() / clockPerPx, 0, 1, height,
                   halfWhite);
}

void handleWheelEventWithModifier(QWheelEvent *event, PxtoneClient *client) {
  if (event->modifiers() & Qt::ControlModifier) {
    bool shift = event->modifiers() & Qt::ShiftModifier;
    QPoint delta = (shift != Settings::SwapZoomOrientation::get()
                        ? event->angleDelta().transposed()
                        : event->angleDelta());
    client->changeEditState(
        [&](EditState &e) {
          e.scale.clockPerPx *= pow(2, delta.x() / 240.0);
          if (e.scale.clockPerPx < 0.5) e.scale.clockPerPx = 0.5;
          if (e.scale.clockPerPx > 128) e.scale.clockPerPx = 128;

          e.scale.pitchPerPx *= pow(2, delta.y() / 240.0);
          if (e.scale.pitchPerPx < 8) e.scale.pitchPerPx = 8;
          if (e.scale.pitchPerPx > PITCH_PER_KEY / 4)
            e.scale.pitchPerPx = PITCH_PER_KEY / 4;
        },
        false);

    event->accept();
  } else if (event->modifiers() & Qt::AltModifier) {
    // In this case, alt flips the scroll direction.
    // Maybe alt shift could handle quantize y?
    qreal delta = event->angleDelta().x();
    int size = sizeof(quantizeXOptions) / sizeof(quantizeXOptions[0]);
    client->changeEditState(
        [&](EditState &e) {
          auto &qx = e.m_quantize_clock_idx;
          if (delta < 0 && qx < size - 1) qx++;
          if (delta > 0 && qx > 0) qx--;
        },
        false);
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

int one_over_last_clock(pxtnService const *pxtn) {
  return pxtn->master->get_beat_clock() * (pxtn->master->get_meas_num() + 1) *
         pxtn->master->get_beat_num();
}

void drawSelection(QPainter &painter, const Interval &interval, qint32 height,
                   double alphaMultiplier) {
  QColor c = slightTint;
  c.setAlpha(c.alpha() * alphaMultiplier);
  painter.fillRect(interval.start, 0, interval.length(), height, c);
  c = halfWhite;
  c.setAlpha(c.alpha() * alphaMultiplier);
  painter.fillRect(interval.start, 0, 1, height, c);
  painter.fillRect(interval.end, 0, 1, height, c);
}

void drawExistingSelection(QPainter &painter, const MouseEditState &state,
                           qreal clockPerPx, qint32 height,
                           double alphaMultiplier) {
  if (state.selection.has_value()) {
    Interval interval{state.selection.value() / clockPerPx};
    drawSelection(painter, interval, height, alphaMultiplier * 0.8);
  }
}

constexpr int32_t tailLineHeight = 4;
void drawUnitBullet(QPainter &painter, int thisX, int y, int w,
                    const QColor &color) {
  painter.fillRect(thisX, y - (tailLineHeight + 6) / 2, 1, (tailLineHeight + 6),
                   color);
  painter.fillRect(thisX + 1, y - (tailLineHeight + 2) / 2, 1,
                   (tailLineHeight + 2), color);
  painter.fillRect(thisX + 2, y - tailLineHeight / 2, std::max(0, w - 3),
                   tailLineHeight, color);
  painter.fillRect(thisX + std::max(0, w - 1), y - (tailLineHeight - 2) / 2, 1,
                   tailLineHeight - 2, color);
}

constexpr int NUM_WIDTH = 8;
constexpr int NUM_HEIGHT = 8;
constexpr int NUM_OFFSET_X = 0;
constexpr int NUM_OFFSET_Y = 40;
constexpr int DARK_NUM_OFFSET_Y = 48;
void drawNum(QPainter *painter, int xr, int y, int num, int num_width,
             int num_height, int num_offset_x, int num_offset_y) {
  static QPixmap images(":/images/images");
  do {
    int digit = num % 10;
    xr -= NUM_WIDTH;
    painter->drawPixmap(xr, y, num_width, num_height, images,
                        num_offset_x + digit * num_width, num_offset_y,
                        num_width, num_height);
    num = (num - digit) / 10;
  } while (num > 0);
}

void drawNumAlignTopRight(QPainter *painter, int xr, int y, int num) {
  drawNum(painter, xr, y, num, NUM_WIDTH, NUM_HEIGHT, NUM_OFFSET_X,
          NUM_OFFSET_Y);
}

static int num_digits(int num) {
  int i = 0;
  do {
    ++i;
    num /= 10;
  } while (num > 0);

  return i;
}

void drawCNumAlignBottomLeft(QPainter *painter, int x, int y, int num) {
  static QPixmap images(":/images/images");
  static int num_width = 9;
  static int num_height = 9;
  static int num_offset_x = num_width;
  static int num_offset_y = 56;
  painter->drawPixmap(x, y - num_height, num_width, num_height, images, 0,
                      num_offset_y, num_width, num_height);
  drawNum(painter, x + (1 + num_digits(num)) * num_width, y - num_height, num,
          num_width, num_height, num_offset_x, num_offset_y);
}

const QColor brightGreen(QColor::fromRgb(0, 240, 128));

const int WINDOW_BOUND_SLACK = 32;
