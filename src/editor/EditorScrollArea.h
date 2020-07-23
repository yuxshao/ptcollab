#ifndef EDITORSCROLLAREA_H
#define EDITORSCROLLAREA_H

#include <QObject>
#include <QScrollArea>
#include <QWheelEvent>

class EditorScrollArea : public QScrollArea {
  Q_OBJECT
 public:
  EditorScrollArea(QWidget *parent, bool match_scale);
  void wheelEvent(QWheelEvent *event) override;
  void controlScroll(QScrollArea *scrollToControl, Qt::Orientation direction);
  void ensureVisible(int x, int xmargin);

 protected:
  void keyPressEvent(QKeyEvent *event) override;

 private:
  bool middleDown;
  bool m_match_scale;
  QPoint lastPos;
  bool event(QEvent *e) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
};

#endif  // EDITORSCROLLAREA_H
