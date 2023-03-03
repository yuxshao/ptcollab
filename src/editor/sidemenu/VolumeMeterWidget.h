#ifndef VOLUMEMETERWIDGET_H
#define VOLUMEMETERWIDGET_H

#include <QFrame>

#include "editor/PxtoneClient.h"
#include "editor/views/Animation.h"

class VolumeMeterFrame : public QWidget {
  Q_OBJECT

  QColor borderColor;
  const QColor getBorderColor() { return borderColor; }
  void setBorderColor(const QColor &c) { borderColor = c; }
  Q_PROPERTY(QColor borderColor READ getBorderColor WRITE setBorderColor
                 DESIGNABLE true)

  QString borderThickness;
  const QString getBorderThickness() { return borderThickness; }
  void setBorderThickness(const QString &s) { borderThickness = s; }
  Q_PROPERTY(QString borderThickness READ getBorderThickness WRITE
                 setBorderThickness DESIGNABLE true)
  int minBaseHeight = 0;

 public:
  explicit VolumeMeterFrame(const PxtoneClient *client,
                            QWidget *parent = nullptr);
  int dbToX(const double db, const int width);
  void resetPeaks();
  int getRealBorderThickness() const;

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
  void refreshShowText();

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
  void refreshShowText();

 signals:
};

#endif  // VOLUMEMETERWIDGET_H
