#include "ViewHelper.h"

#include <QWheelEvent>

#include "editor/ComboOptions.h"
#include "editor/Settings.h"
#include "editor/StyleEditor.h"
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
  painter.setFont(
      QFont(StyleEditor::config.font.EditorFont, Settings::TextSize::get()));
  painter.drawText(position + QPoint(8, 13),
                   QString("%1 (%2)").arg(username).arg(uid));
}

QColor makeTranslucent(const QColor &c, int divisor) {
  return QColor{c.red(), c.green(), c.blue(), c.alpha() / divisor};
}

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
  QColor color = Settings::EditorRecording::get()
                     ? StyleEditor::config.color.PlayheadRecording
                     : StyleEditor::config.color.Playhead;
  if (!moo_clock->this_seek_caught_up() || moo_clock->now() <= 0)
    color = makeTranslucent(color, 2);
  const int x = moo_clock->now() / clockPerPx;
  drawPlayhead(painter, x, height, color, drawHead);
}

void drawLastSeek(QPainter &painter, const PxtoneClient *client, qint32 height,
                  bool drawHead) {
  if (client->following_uid() == client->uid()) {
    QColor color = StyleEditor::config.color.Playhead;
    color.setAlpha(color.alpha() / 2);
    drawPlayhead(painter,
                 client->lastSeek() / client->editState().scale.clockPerPx,
                 height, color, drawHead);
  }
}

void drawRepeatAndEndBars(QPainter &painter, const MooClock *moo_clock,
                          qreal clockPerPx, int height) {
  if (moo_clock->has_last())
    painter.fillRect(moo_clock->last_clock() / clockPerPx, 0, 1, height,
                     makeTranslucent(StyleEditor::config.color.Playhead, 2));

  painter.fillRect(moo_clock->repeat_clock() / clockPerPx, 0, 1, height,
                   makeTranslucent(StyleEditor::config.color.Playhead, 2));
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
    // ^ oh lord
    qreal delta = event->angleDelta().x();
    client->changeEditState(
        [&](EditState &e) {
          auto &qx = e.m_quantize_clock_idx;
          if (delta < 0 && qx < int(quantizeXOptions().size()) - 1) qx++;
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
  QColor color = makeTranslucent(StyleEditor::config.color.Playhead, 8);
  color.setAlpha(color.alpha() * alphaMultiplier);
  painter.fillRect(interval.start, 0, interval.length(), height, color);
  color = makeTranslucent(StyleEditor::config.color.Playhead, 2);
  color.setAlpha(color.alpha() * alphaMultiplier);
  painter.fillRect(interval.start, 0, 1, height, color);
  painter.fillRect(interval.end, 0, 1, height, color);
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
void fillUnitBullet(QPainter &painter, int thisX, int y, int w,
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

void drawUnitBullet(QPainter &painter, int thisX, int y, int w) {
  // Drawing a rectangle outline since outlining the full bullet is complicated
  // + doesn't actually look that great
  painter.drawRect(thisX, y - (tailLineHeight + 2) / 2, std::max(0, w - 1),
                   (tailLineHeight + 1));
}

constexpr int NUM_WIDTH = 8;
constexpr int NUM_HEIGHT = 8;
constexpr int NUM_OFFSET_X = 0;
constexpr int NUM_OFFSET_Y = 40;
void drawNum(QPainter *painter, int xr, int y, int num, int num_width,
             int num_height, int num_offset_x, int num_offset_y) {
  do {
    int digit = num % 10;
    xr -= NUM_WIDTH;
    painter->drawPixmap(
        xr, y, num_width, num_height, *StyleEditor::measureImages(),
        num_offset_x + digit * num_width, num_offset_y, num_width, num_height);
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

void drawOctaveNumAlignBottomLeft(QPainter *painter, int x, int y, int num,
                                  int height, bool a) {
  QPixmap &images = *StyleEditor::measureImages();
  if (height > 10) {
    bool big = height > 13;
    int num_width = (big ? 9 : 8);
    int num_height = (big ? 9 : 6);
    int num_offset_x = num_width * 2;
    int num_offset_y = (big ? 56 : 49);
    painter->drawPixmap(x, y - num_height, num_width, num_height, images,
                        (a ? 0 : num_width), num_offset_y, num_width,
                        num_height);
    drawNum(painter, x + (1 + num_digits(num)) * num_width, y - num_height, num,
            num_width, num_height, num_offset_x, num_offset_y);
  } else if (height >= 7)
    painter->drawPixmap(x - 2, y - (height + 1) / 2, 6, 6, images, 96, 48, 6,
                        6);
}

const int LEFT_LEGEND_WIDTH = 40;
QTransform worldTransform() {
  return QTransform().translate(LEFT_LEGEND_WIDTH, 0);
}

const int WINDOW_BOUND_SLACK = 32;
