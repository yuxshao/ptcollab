#include "ParamView.h"

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
constexpr int BACKGROUND_GAPS[] = {0, 24, 32, 64, 96, 104, 1000};
static const QColor *GAP_COLORS[] = {&darkBlue, &darkBlue, &blue,
                                     &blue,     &darkBlue, &darkBlue};
constexpr int NUM_BACKGROUND_GAPS =
    sizeof(BACKGROUND_GAPS) / sizeof(BACKGROUND_GAPS[0]);
void ParamView::paintEvent(QPaintEvent *) {
  const pxtnMaster *master = m_client->pxtn()->master;
  QPainter painter(this);
  painter.fillRect(0, 0, size().width(), size().height(), Qt::black);

  // Draw white lines under background
  // TODO: Dedup with keyboardview
  QBrush beatBrush(QColor::fromRgb(128, 128, 128));
  QBrush measureBrush(Qt::white);
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
    painter.fillRect(0, this_y, size().width(),
                     std::max(1, next_y - this_y - 2), *GAP_COLORS[i]);
  }
}
