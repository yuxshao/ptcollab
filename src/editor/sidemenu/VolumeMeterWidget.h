#ifndef VOLUMEMETERWIDGET_H
#define VOLUMEMETERWIDGET_H

#include <QFrame>

#include "editor/PxtoneClient.h"
#include "editor/views/Animation.h"

class VolumeMeterBars : public QWidget {
  Q_OBJECT
 public:
  explicit VolumeMeterBars(const PxtoneClient *client,
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
  explicit VolumeMeterLabels(VolumeMeterBars *frame, QWidget *parent = nullptr);
  void refreshShowText();

 private:
  void paintEvent(QPaintEvent *event) override;
  QSize minimumSizeHint() const override;
  int dbToX(int db);
  VolumeMeterBars *m_bars;
  bool m_show_text;

 signals:
};

class VolumeMeterWidget : public QWidget {
  Q_OBJECT
  VolumeMeterBars *m_bars;
  VolumeMeterLabels *m_labels;

 public:
  explicit VolumeMeterWidget(VolumeMeterBars *bars, QWidget *parent = nullptr);
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void refreshShowText();

 signals:
};

#endif  // VOLUMEMETERWIDGET_H
