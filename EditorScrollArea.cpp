#include "EditorScrollArea.h"

#include <QDebug>

class TransposableWheelEvent : QWheelEvent {
 public:
  void transpose() {
    pixelD = pixelD.transposed();
    angleD = angleD.transposed();
  }
};

EditorScrollArea::EditorScrollArea(QWidget *parent) : QScrollArea(parent) {
  setWidgetResizable(true);
}

// TODO: Maybe a class that converts wheel event to action
void EditorScrollArea::wheelEvent(QWheelEvent *event) {
  // Maybe scroll the other dimension.
  if (event->modifiers() & Qt::ShiftModifier) {
    ((TransposableWheelEvent *)(event))->transpose();
    // Disable modifier because QScrollArea takes it to mean scroll by a page
    event->setModifiers(event->modifiers() & ~Qt::ShiftModifier);
  }

  QScrollArea::wheelEvent(event);
}
