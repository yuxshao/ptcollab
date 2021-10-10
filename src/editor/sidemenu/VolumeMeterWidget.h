#ifndef VOLUMEMETERWIDGET_H
#define VOLUMEMETERWIDGET_H

#include <QFrame>

#include "editor/PxtoneClient.h"
#include "editor/views/Animation.h"

class VolumeMeterWidget : public QFrame {
  Q_OBJECT
 public:
  explicit VolumeMeterWidget(const PxtoneClient *client,
                             QWidget *parent = nullptr);

 private:
  void paintEvent(QPaintEvent *event) override;
  int dbToX(double db);
  QSize minimumSizeHint() const override;
  const PxtoneClient *m_client;
  Animation *m_animation;

 signals:
};

#endif  // VOLUMEMETERWIDGET_H
