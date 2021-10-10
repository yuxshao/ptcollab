#include "VolumeMeterWidget.h"

#include <QApplication>
#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
const int MIN_DB = -36;
const int MAX_DB = 3;

// TODO: make this colour dynamic
const static QColor BGCOLOR = QColor::fromRgb(26, 25, 73);           // #1A1949
const static QColor BGCOLOR_SOFT = QColor::fromRgb(26, 25, 73, 30);  // #1A1949
const static QColor BAR_COLOR = QColor::fromRgb(0, 240, 128);        // #00F080
const static QColor BAR_COLOR_SOFT = QColor::fromRgb(0, 240, 128, 20);
const static QColor BAR_HIGH_COLOR = Qt::red;

VolumeMeterWidget::VolumeMeterWidget(const PxtoneClient *client,
                                     QWidget *parent)
    : QFrame(parent), m_client(client), m_animation(new Animation(this)) {
  setFrameStyle(QFrame::Panel | QFrame::Sunken);
  connect(m_animation, &Animation::nextFrame, [this]() { update(); });
}

void VolumeMeterWidget::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  p.fillRect(e->rect(), BGCOLOR);
  int w_limit = dbToX(-3);

  const auto &levels = m_client->volumeLevels();
  for (uint i = 0; i < levels.size(); ++i) {
    int w = dbToX(levels[i].current_volume_dbfs());
    int y = (height() + 1) * i / levels.size();
    int h = (height() + 1) / levels.size() - 1;
    if (w_limit > w)
      p.fillRect(QRect(0, y, w, h), BAR_COLOR);
    else {
      p.fillRect(QRect(0, y, w_limit, h), BAR_COLOR);
      p.fillRect(QRect(w_limit, y, w - w_limit, h), BAR_HIGH_COLOR);
    }
  }
  for (int db = MIN_DB; db < MAX_DB; db += 3)
    p.fillRect(dbToX(db), 0, 1, height(), BGCOLOR_SOFT);
  p.fillRect(dbToX(-3), 0, 1, height(), BGCOLOR);

  p.setFont(QFont("Sans serif", 6));
  for (int db = MIN_DB + 6; db < MAX_DB; db += 6) {
    // p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.fillRect(dbToX(db), 0, 1, height(), BGCOLOR_SOFT);
    p.setPen(BGCOLOR_SOFT);
    int baseX = dbToX(db) - 20;

    for (int x = -1; x <= 1; ++x)
      for (int y = -1; y <= 1; ++y) {
        p.drawText(QRect(baseX + x, y, 40, height()), Qt::AlignCenter,
                   QString("%1").arg(db));
      }

    // p.setCompositionMode(QPainter::CompositionMode_Plus);
    p.setPen(Qt::white);
    p.drawText(QRect(baseX, 0, 40, height()), Qt::AlignCenter,
               QString("%1").arg(db));
  }
  QFrame::paintEvent(e);
}

int VolumeMeterWidget::dbToX(double db) {
  return std::clamp<int>((db - MIN_DB) * width() / (MAX_DB - MIN_DB), 0,
                         width());
}

QSize VolumeMeterWidget::minimumSizeHint() const { return QSize(0, 16); }
