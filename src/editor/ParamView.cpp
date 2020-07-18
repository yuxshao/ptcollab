#include "ParamView.h"

#include <QPaintEvent>
#include <QPainter>

ParamView::ParamView(PxtoneClient *client, QWidget *parent)
    : QWidget(parent), m_client(client), m_anim(new Animation(this)) {
  // TODO: dedup with keyboardview
  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  updateGeometry();
  setMouseTracking(true);
  connect(m_anim, &Animation::nextFrame, [this]() { update(); });
  connect(m_client, &PxtoneClient::measureNumChanged,
          [this]() { updateGeometry(); });
}

// TODO: Don't repeat
static int one_over_last_clock(pxtnService const *pxtn) {
  return pxtn->master->get_beat_clock() * (pxtn->master->get_meas_num() + 1) *
         pxtn->master->get_beat_num();
}

QSize ParamView::sizeHint() const {
  return QSize(one_over_last_clock(m_client->pxtn()) /
                   m_client->editState().scale.clockPerPx,
               0x20);
}

static const QColor blue(QColor::fromRgb(52, 50, 85));
static const QColor darkBlue(QColor::fromRgb(26, 25, 73));
constexpr int BACKGROUND_GAPS[] = {-1000, 24, 32, 64, 96, 104, 1000};
static const QColor *GAP_COLORS[] = {&darkBlue, &darkBlue, &blue,
                                     &blue,     &darkBlue, &darkBlue};

static const QColor darkTeal(QColor::fromRgb(0, 96, 96));
static const QColor brightGreen(QColor::fromRgb(0, 240, 128));

constexpr int NUM_BACKGROUND_GAPS =
    sizeof(BACKGROUND_GAPS) / sizeof(BACKGROUND_GAPS[0]);
constexpr int WINDOW_BOUND_SLACK = 32;

void ParamView::paintEvent(QPaintEvent *event) {
  const pxtnService *pxtn = m_client->pxtn();
  Interval clockBounds = {
      qint32(event->rect().left() * m_client->editState().scale.clockPerPx) -
          WINDOW_BOUND_SLACK,
      qint32(event->rect().right() * m_client->editState().scale.clockPerPx) +
          WINDOW_BOUND_SLACK};
  QPainter painter(this);
  painter.fillRect(0, 0, size().width(), size().height(), Qt::black);

  // Draw white lines under background
  // TODO: Dedup with keyboardview
  QBrush beatBrush(QColor::fromRgb(128, 128, 128));
  QBrush measureBrush(Qt::white);
  const pxtnMaster *master = pxtn->master;
  for (int beat = 0; true; ++beat) {
    bool isMeasureLine = (beat % master->get_beat_num() == 0);
    int x = master->get_beat_clock() * beat /
            m_client->editState().scale.clockPerPx;
    if (x > size().width()) break;
    painter.fillRect(x, 0, 1, size().height(),
                     (isMeasureLine ? measureBrush : beatBrush));
  }

  // Draw param background
  for (int i = 0; i < NUM_BACKGROUND_GAPS - 1; ++i) {
    int this_y = BACKGROUND_GAPS[i] * size().height() / 0x80;
    int next_y = BACKGROUND_GAPS[i + 1] * size().height() / 0x80;
    painter.fillRect(0, this_y + 1, size().width(),
                     std::max(1, next_y - this_y - 2), *GAP_COLORS[i]);
  }

  {
    int32_t height = 4;
    int32_t width = 2;
    EVENTKIND current_kind = EVENTKIND_VOLUME;
    int32_t last_value = EVENTDEFAULT_VOLUME;
    int32_t last_clock = -1;
    for (const EVERECORD *e = pxtn->evels->get_Records(); e != nullptr;
         e = e->next) {
      if (e->clock > clockBounds.end) break;

      if (e->kind != current_kind) continue;
      int unit_no = e->unit_no;
      qint32 unit_id = m_client->unitIdMap().noToId(unit_no);
      bool matchingUnit = (unit_id == m_client->editState().m_current_unit_id);
      if (!matchingUnit) continue;

      int32_t lastX = last_clock / m_client->editState().scale.clockPerPx;
      int32_t lastY = last_value * size().height() / 0x80;
      int32_t thisX = e->clock / m_client->editState().scale.clockPerPx;
      int32_t thisY = e->value * size().height() / 0x80;

      painter.fillRect(lastX, lastY - height / 2, thisX - lastX, height,
                       darkTeal);
      painter.fillRect(thisX, std::min(lastY, thisY) - height / 2, width,
                       std::max(lastY, thisY) - std::min(lastY, thisY) + height,
                       darkTeal);
      painter.fillRect(lastX, lastY - height / 2, width, height, brightGreen);

      last_value = e->value;
      last_clock = e->clock;
    }
    int32_t lastX = last_clock / m_client->editState().scale.clockPerPx;
    int32_t lastY = last_value * size().height() / 0x80;
    painter.fillRect(lastX, lastY - height / 2, size().width() - lastX, height,
                     darkTeal);
    painter.fillRect(lastX, lastY - height / 2, width, height, brightGreen);
  }
}
