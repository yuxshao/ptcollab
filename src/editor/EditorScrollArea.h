#ifndef EDITORSCROLLAREA_H
#define EDITORSCROLLAREA_H

#include <QObject>
#include <QScrollArea>
#include <QWheelEvent>

#include "editor/views/Animation.h"

class EditorScrollArea : public QScrollArea {
  Q_OBJECT
 public:
  EditorScrollArea(QWidget *parent, bool match_scale);
  void wheelEvent(QWheelEvent *event) override;
  void controlScroll(QScrollArea *scrollToControl, Qt::Orientation direction);
  void ensureWithinMargin(int x, qreal minDistFromLeft,
                          qreal jumpMinDistFromLeft, qreal jumpMaxDistFromLeft,
                          qreal maxDistFromLeft);
  void setEnableScrollWithMouseX(bool);
 signals:
  void viewportChanged(const QRect &viewport);

 protected:
  void keyPressEvent(QKeyEvent *event) override;

 private:
  bool mouseDown;
  bool m_match_scale;
  QPoint lastPos;
  Animation *anim;
  bool m_scroll_with_mouse_x;
  bool event(QEvent *e) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void scrollWithMouseX();
  QRect viewportRect();
  void updateMouseDownState(QMouseEvent *event);
};

#endif  // EDITORSCROLLAREA_H
