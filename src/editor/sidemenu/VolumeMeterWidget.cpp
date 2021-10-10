#include "VolumeMeterWidget.h"

#include <QVBoxLayout>

VolumeMeterWidget::VolumeMeterWidget(VolumeMeterFrame *meter, QWidget *parent)
    : QWidget(parent), m_meter(meter) {
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
