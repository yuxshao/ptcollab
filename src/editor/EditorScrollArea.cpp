#include "EditorScrollArea.h"

#include <QDebug>
#include <QScrollBar>

class TransposableWheelEvent : QWheelEvent {
 public:
  void transpose() {
    // not using QPoint::transposed() because it doesn't exist in older versions
    pixelD = {pixelD.y(), pixelD.x()};
    angleD = {angleD.y(), angleD.x()};
  }
};

EditorScrollArea::EditorScrollArea(QWidget *parent)
    : QScrollArea(parent), middleDown(false), lastPos() {
  setWidgetResizable(true);
  setMouseTracking(true);
}

void EditorScrollArea::mousePressEvent(QMouseEvent *event) {
  // double ratioH = double(lastPos.x()) / viewport()->width();

  /*qDebug() << horizontalScrollBar()->pageStep()
           << horizontalScrollBar()->value() << horizontalScrollBar()->maximum()
           << ratioH;*/
  if (event->button() & Qt::MiddleButton) {
    middleDown = true;
    viewport()->setCursor(Qt::ClosedHandCursor);
    event->accept();
  }
  lastPos = event->pos();
}

static int newPos(double oldMax, int oldWidth, int oldPos, double newMax,
                  int newWidth, double ratio) {
  double newMaxToOldMax = (newMax + newWidth) / double(oldMax + oldWidth);
  return newMaxToOldMax * oldPos +
         (newMaxToOldMax * oldWidth - newWidth) * ratio;
}

bool EditorScrollArea::event(QEvent *e) {
  // Capture the old scrollbar positions and manually reconfigure the new ones
  // so that a certain position on the screen is preserved in case of resize.
  int oldMaxH = horizontalScrollBar()->maximum();
  int oldWidthH = horizontalScrollBar()->pageStep();
  int oldH = horizontalScrollBar()->value();
  double ratioH = double(lastPos.x()) / viewport()->width();

  int oldMaxV = verticalScrollBar()->maximum();
  int oldWidthV = verticalScrollBar()->pageStep();
  int oldV = verticalScrollBar()->value();
  double ratioV = double(lastPos.y()) / viewport()->height();

  bool result = QScrollArea::event(e);
  int newMaxH = horizontalScrollBar()->maximum();
  int newWidthH = horizontalScrollBar()->pageStep();
  int newH = newPos(oldMaxH, oldWidthH, oldH, newMaxH, newWidthH, ratioH);
  int newMaxV = verticalScrollBar()->maximum();
  int newWidthV = verticalScrollBar()->pageStep();
  int newV = newPos(oldMaxV, oldWidthV, oldV, newMaxV, newWidthV, ratioV);
  if (newH != oldH) horizontalScrollBar()->setValue(newH);
  if (newV != oldV) verticalScrollBar()->setValue(newV);
  return result;
}
void EditorScrollArea::mouseReleaseEvent(QMouseEvent *event) {
  event->ignore();
  if (event->button() & Qt::MiddleButton) {
    middleDown = false;
    viewport()->unsetCursor();
    event->accept();
  }
  lastPos = event->pos();
}
void EditorScrollArea::mouseMoveEvent(QMouseEvent *event) {
  event->ignore();
  if (middleDown) {
    // Copied from qgraphicsview
    QScrollBar *hBar = horizontalScrollBar();
    QScrollBar *vBar = verticalScrollBar();
    QPoint delta = event->pos() - lastPos;
    hBar->setValue(hBar->value() + (isRightToLeft() ? delta.x() : -delta.x()));
    vBar->setValue(vBar->value() - delta.y());
    event->accept();
  }
  lastPos = event->pos();
}
// TODO: Maybe a class that converts wheel event to action
void EditorScrollArea::wheelEvent(QWheelEvent *event) {
  // Maybe scroll the other dimension.
  if (event->modifiers() & Qt::ShiftModifier) {
    ((TransposableWheelEvent *)(event))->transpose();
    // Disable modifier because QScrollArea takes it to mean scroll by a page
    event->setModifiers(event->modifiers() & ~Qt::ShiftModifier);
  }

  // QT by default has alt scroll horizontally, but pxtone uses shift.
  event->setModifiers(event->modifiers() & ~Qt::AltModifier);

  QScrollArea::wheelEvent(event);
}
