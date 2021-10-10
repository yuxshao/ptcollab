#include "VolumeMeterWidget.h"

#include <QApplication>
#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>
const int MIN_DB = -36;
const int MID_DB = -6;
const int HIGH_DB = 0;
const int MAX_DB = 3;

// TODO: make this colour dynamic
const static QColor BGCOLOR = QColor::fromRgb(26, 25, 73);           // #1A1949
const static QColor BGCOLOR_SOFT = QColor::fromRgb(26, 25, 73, 30);  // #1A1949
const static QColor BAR_COLOR = QColor::fromRgb(0, 240, 128);        // #00F080
const static QColor BAR_MID_COLOR = QColor::fromRgb(255, 255, 128);  // #FFFF80
const static QColor LABEL_COLOR = QColor::fromRgb(210, 202, 156);    // #D2CA9C
const static QColor TICK_COLOR = QColor::fromRgb(52, 50, 65);        // #343241
const static QColor BAR_HIGH_COLOR = Qt::red;

static QLinearGradient barGradient() {
  static QLinearGradient g = []() {
    QLinearGradient g(0, 0, 1, 0);
    g.setCoordinateMode(QGradient::ObjectMode);
    g.setColorAt(0, BAR_COLOR);
    g.setColorAt(double(MID_DB - MIN_DB) / (MAX_DB - MIN_DB), BAR_MID_COLOR);
    g.setColorAt(double(HIGH_DB - MIN_DB) / (MAX_DB - MIN_DB), BAR_HIGH_COLOR);
    return g;
  }();
  return g;
}

VolumeMeterFrame::VolumeMeterFrame(const PxtoneClient *client, QWidget *parent)
    : QFrame(parent), m_client(client), m_animation(new Animation(this)) {
  setFrameStyle(QFrame::Panel | QFrame::Sunken);
  connect(m_animation, &Animation::nextFrame, this, [this]() { update(); });
}

void VolumeMeterFrame::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  p.fillRect(e->rect(), BGCOLOR);
  // int w_limit = dbToX(-3);

  const auto &levels = m_client->volumeLevels();
  for (uint i = 0; i < levels.size(); ++i) {
    int w = dbToX(levels[i].current_volume_dbfs());
    int y = (height() + 1) * i / levels.size();
    int h = (height() + 1) / levels.size() - 1;
    p.fillRect(QRect(0, y, width(), h), barGradient());
    // p.fillRect(QRect(0, y, width(), h), BAR_HIGH_COLOR);
    // p.fillRect(QRect(0, y, dbToX(HIGH_DB), h), BAR_MID_COLOR);
    // p.fillRect(QRect(0, y, dbToX(MID_DB), h), BAR_COLOR);

    p.fillRect(QRect(w, y, width() - w, h), BGCOLOR);
  }
  for (int db = MIN_DB; db < MAX_DB; db += 3)
    p.fillRect(dbToX(db), 0, 1, height(), BGCOLOR_SOFT);
  p.fillRect(dbToX(-3), 0, 1, height(), BGCOLOR_SOFT);
  p.fillRect(dbToX(-3), 0, 1, height(), BGCOLOR_SOFT);
  QFrame::paintEvent(e);
}

int VolumeMeterFrame::dbToX(double db) {
  return std::clamp<int>((db - MIN_DB) * width() / (MAX_DB - MIN_DB), 0,
                         width());
}

QSize VolumeMeterFrame::minimumSizeHint() const { return QSize(0, 13); }

VolumeMeterLabels::VolumeMeterLabels(VolumeMeterFrame *frame, QWidget *parent)
    : QWidget(parent), m_frame(frame) {}

constexpr int TICK_HEIGHT = 4;
constexpr int SMALL_TICK_HEIGHT = 2;

void VolumeMeterLabels::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  p.setFont(QFont("Sans serif", 6));
  p.setPen(LABEL_COLOR);

  p.drawText(QRect(0, 0, width() - 2, height() - TICK_HEIGHT),
             (Qt::AlignRight | Qt::AlignTop), "dB");
  for (int db = MIN_DB + 6; db < MAX_DB; db += 6) {
    int x = m_frame->dbToX(db);
    p.drawText(QRect(x - 20, 0, 40, height() - TICK_HEIGHT),
               (Qt::AlignHCenter | Qt::AlignTop), QString("%1").arg(db));
  }
  for (int db = MIN_DB; db < MAX_DB; db += 1) {
    int x = m_frame->dbToX(db);
    int h = (db % 6 == 0 ? TICK_HEIGHT : SMALL_TICK_HEIGHT);
    p.fillRect(x, height() - h, 1, h, TICK_COLOR);
  }
  QWidget::paintEvent(e);
}

QSize VolumeMeterLabels::minimumSizeHint() const { return QSize(0, 16); }

VolumeMeterWidget::VolumeMeterWidget(VolumeMeterFrame *meter, QWidget *parent)
    : QWidget(parent) {
  QVBoxLayout *layout = new QVBoxLayout;
  layout->setMargin(0);
  layout->setSpacing(0);
  setLayout(layout);
  layout->addWidget(new VolumeMeterLabels(meter, this));
  layout->addWidget(meter);

  meter->setSizePolicy(
      QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
  meter->setParent(this);
}
