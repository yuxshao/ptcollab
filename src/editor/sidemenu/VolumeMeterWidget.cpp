#include "VolumeMeterWidget.h"

#include <QDebug>
#include <QPaintEvent>
#include <QPainter>

const int MIN_DB = -36;
const int MAX_DB = 3;
VolumeMeterWidget::VolumeMeterWidget(const PxtoneClient *client,
                                     QWidget *parent)
    : QFrame(parent), m_client(client), m_animation(new Animation(this)) {
  connect(m_animation, &Animation::nextFrame, [this]() { update(); });
}

void VolumeMeterWidget::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  width();
  const auto &levels = m_client->volumeLevels();
  for (uint i = 0; i < levels.size(); ++i) {
    int w = std::clamp<int>((levels[i].current_volume_dbfs() - MIN_DB) *
                                width() / (MAX_DB - MIN_DB),
                            0, width());
    p.fillRect(
        QRect(0, height() * i / levels.size(), w, height() / levels.size()),
        Qt::green);
  }
  QFrame::paintEvent(event);
}

QSize VolumeMeterWidget::minimumSizeHint() const { return QSize(0, 16); }
