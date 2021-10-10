#ifndef VOLUMEMETERWIDGET_H
#define VOLUMEMETERWIDGET_H

#include <QFrame>

#include "editor/PxtoneClient.h"
#include "editor/views/Animation.h"

class VolumeMeterFrame : public QFrame {
  Q_OBJECT
 public:
  explicit VolumeMeterFrame(const PxtoneClient *client,
                            QWidget *parent = nullptr);
  int dbToX(double db);
  void resetPeaks();

 private:
  void paintEvent(QPaintEvent *event) override;

  QSize minimumSizeHint() const override;
  const PxtoneClient *m_client;
  Animation *m_animation;
  std::vector<double> m_peaks;

 signals:
};

class VolumeMeterLabels : public QWidget {
  Q_OBJECT

 public:
  explicit VolumeMeterLabels(VolumeMeterFrame *frame,
                             QWidget *parent = nullptr);
  void toggleText();

 private:
  void paintEvent(QPaintEvent *event) override;
  QSize minimumSizeHint() const override;
  VolumeMeterFrame *m_frame;
  bool m_show_text;

 signals:
};

class VolumeMeterWidget : public QWidget {
  Q_OBJECT
  VolumeMeterFrame *m_frame;
  VolumeMeterLabels *m_labels;

 public:
  explicit VolumeMeterWidget(VolumeMeterFrame *meter,
                             QWidget *parent = nullptr);
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;

 signals:
};

#endif  // VOLUMEMETERWIDGET_H
