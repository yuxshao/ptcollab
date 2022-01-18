#ifndef MEASUREVIEW_H
#define MEASUREVIEW_H

#include <QMenu>
#include <QWidget>

#include "Animation.h"
#include "MooClock.h"
#include "editor/PxtoneClient.h"
#include "editor/audio/NotePreview.h"

class MeasureView : public QWidget {
  Q_OBJECT

  PxtoneClient *m_client;
  Animation *m_anim;
  Scale m_last_scale;
  MooClock *m_moo_clock;
  QFont m_label_font;
  std::unique_ptr<NotePreview> m_audio_note_preview;
  std::optional<int> m_hovered_unit_no;

  // Tracked separately from editState since that one doesn't track leaves +
  // this one is relative to window position
  std::optional<int> m_mouse_x;

  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void moveEvent(QMoveEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void wheelEvent(QWheelEvent *event) override;
  QSize sizeHint() const override;

  void handleNewEditState(const EditState &e);

 public:
  explicit MeasureView(PxtoneClient *client, MooClock *moo_clock,
                       QWidget *parent = nullptr);
  void setFocusedUnit(std::optional<int> unit_no);

 signals:
  void heightChanged(int h);
  void hoverUnitNoChanged(std::optional<int>);
};

#endif  // MEASUREVIEW_H
