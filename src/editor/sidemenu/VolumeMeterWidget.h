#ifndef VOLUMEMETERWIDGET_H
#define VOLUMEMETERWIDGET_H

#include <QProgressBar>

class VolumeMeterWidget : public QProgressBar {
  Q_OBJECT
 public:
  explicit VolumeMeterWidget(QWidget *parent = nullptr);

 signals:
};

#endif  // VOLUMEMETERWIDGET_H
