#include "EditorScrollArea.h"

#include <QDebug>

class TransposableWheelEvent : QWheelEvent {
 public:
  void transpose() {
    pixelD = pixelD.transposed();
    angleD = angleD.transposed();
  }
};

EditorScrollArea::EditorScrollArea(QWidget *parent) : QScrollArea(parent) {}

// TODO: Maybe a class that converts wheel event to action
void EditorScrollArea::wheelEvent(QWheelEvent *event) {
  if (event->modifiers() & Qt::ControlModifier)
    return;  // This changes zoom in the child

  // Maybe scroll the other dimension
  if (event->modifiers() & Qt::ShiftModifier)
    ((TransposableWheelEvent *)(event))->transpose();

  QScrollArea::wheelEvent(event);
}
