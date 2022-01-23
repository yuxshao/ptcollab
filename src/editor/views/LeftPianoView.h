#ifndef LEFTPIANOVIEW_H
#define LEFTPIANOVIEW_H
#include <QWidget>

#include "Animation.h"
#include "MooClock.h"
#include "editor/PxtoneClient.h"

class LeftPianoView : public QWidget {
  Q_OBJECT

  void paintEvent(QPaintEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  QSize sizeHint() const override;
  Animation *m_anim;
  PxtoneClient *m_client;
  MooClock *m_moo_clock;
  Scale m_last_scale;

 public:
  explicit LeftPianoView(PxtoneClient *client, MooClock *moo_clock,
                         QWidget *parent = nullptr);
};
#endif  // LEFTPIANOVIEW_H
