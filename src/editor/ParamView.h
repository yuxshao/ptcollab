#ifndef PARAMVIEW_H
#define PARAMVIEW_H

#include <QWidget>

#include "Animation.h"
#include "PxtoneClient.h"

class ParamView : public QWidget {
  Q_OBJECT

  PxtoneClient *m_client;
  Animation *m_anim;
  Scale m_last_scale;

  void paintEvent(QPaintEvent *event) override;
  QSize sizeHint() const override;

 public:
  explicit ParamView(PxtoneClient *client, QWidget *parent = nullptr);

 signals:
};

#endif  // PARAMVIEW_H
