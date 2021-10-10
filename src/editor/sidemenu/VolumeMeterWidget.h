#ifndef VOLUMEMETERWIDGET_H
#define VOLUMEMETERWIDGET_H

#include <QWidget>

#include "VolumeMeterFrame.h"

class VolumeMeterWidget : public QWidget {
  Q_OBJECT
 private:
  VolumeMeterFrame *m_meter;

 public:
  explicit VolumeMeterWidget(VolumeMeterFrame *meter,
                             QWidget *parent = nullptr);

 signals:
};

#endif  // VOLUMEMETERWIDGET_H
