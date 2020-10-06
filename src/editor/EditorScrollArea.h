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
  void ensureWithinMargin(int x, qreal minDistFromLeft,
                          qreal jumpMinDistFromLeft, qreal jumpMaxDistFromLeft,
                          qreal maxDistFromLeft);
 signals:
  void viewportChanged(const QRect &viewport);

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
  void resizeEvent(QResizeEvent *event) override;
  QRect viewportRect();
};

#endif  // EDITORSCROLLAREA_H
