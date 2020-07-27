#ifndef PARAMVIEW_H
#define PARAMVIEW_H

#include <QWidget>

#include "Animation.h"
#include "MooClock.h"
#include "editor/PxtoneClient.h"
#include "editor/audio/NotePreview.h"

class ParamView : public QWidget {
  Q_OBJECT

  PxtoneClient *m_client;
  Animation *m_anim;
  Scale m_last_scale;
  MooClock *m_moo_clock;
  std::unique_ptr<NotePreview> m_audio_note_preview;

  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  QSize sizeHint() const override;

 public:
  explicit ParamView(PxtoneClient *client, MooClock *moo_clock,
                     QWidget *parent = nullptr);

 signals:
};

#endif  // PARAMVIEW_H
