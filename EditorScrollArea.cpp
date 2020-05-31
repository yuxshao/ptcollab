#include "EditorScrollArea.h"

#include <QDebug>
#include <QScrollBar>

class TransposableWheelEvent : QWheelEvent {
 public:
  void transpose() {
    pixelD = pixelD.transposed();
    angleD = angleD.transposed();
  }
};

EditorScrollArea::EditorScrollArea(QWidget *parent)
    : QScrollArea(parent), middleDown(false), lastPos() {
  setWidgetResizable(true);
  setMouseTracking(true);
}

void EditorScrollArea::mousePressEvent(QMouseEvent *event) {
  if (event->button() & Qt::MiddleButton) {
    middleDown = true;
    viewport()->setCursor(Qt::ClosedHandCursor);
    event->accept();
  }
  lastPos = event->pos();
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
