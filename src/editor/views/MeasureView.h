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
  QFont m_label_font;
  QFontMetrics m_label_font_metrics;
  std::unique_ptr<NotePreview> m_audio_note_preview;
  std::optional<int> m_focused_unit_no;
  std::optional<int> m_hovered_unit_no;
  std::vector<std::optional<std::pair<QString, QPixmap>>> m_pinned_unit_labels;
  bool m_selection_mode;
  bool m_jump_to_unit_enabled;

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
  void setHoveredUnitNo(std::optional<int>);

 public:
  explicit MeasureView(PxtoneClient *client, QWidget *parent = nullptr);
  void setFocusedUnit(std::optional<int> unit_no);
  void setSelectUnitEnabled(bool);

 signals:
  void heightChanged(int h);
  void hoverUnitNoChanged(std::optional<int>, bool);
};

#endif  // MEASUREVIEW_H
