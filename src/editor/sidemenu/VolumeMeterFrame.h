#ifndef VOLUMEMETERFRAME_H
#define VOLUMEMETERFRAME_H

#include <QFrame>

#include "editor/PxtoneClient.h"
#include "editor/views/Animation.h"

class VolumeMeterFrame : public QFrame {
  Q_OBJECT
 public:
  explicit VolumeMeterFrame(const PxtoneClient *client,
                            QWidget *parent = nullptr);
  int dbToX(double db);

 private:
  void paintEvent(QPaintEvent *event) override;

  QSize minimumSizeHint() const override;
  const PxtoneClient *m_client;
  Animation *m_animation;

 signals:
};

class VolumeMeterLabels : public QWidget {
  Q_OBJECT

 public:
  explicit VolumeMeterLabels(VolumeMeterFrame *frame,
                             QWidget *parent = nullptr);

 private:
  void paintEvent(QPaintEvent *event) override;
  QSize minimumSizeHint() const override;
  VolumeMeterFrame *m_frame;

 signals:
};

#endif  // VOLUMEMETERFRAME_H
