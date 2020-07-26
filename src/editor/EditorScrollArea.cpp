#include "EditorScrollArea.h"

#include <QDebug>
#include <QScrollBar>
#include <QStyle>

class TransposableWheelEvent : QWheelEvent {
 public:
  void transpose() {
    // not using QPoint::transposed() because it doesn't exist in older versions
    pixelD = {pixelD.y(), pixelD.x()};
    angleD = {angleD.y(), angleD.x()};
  }
};

EditorScrollArea::EditorScrollArea(QWidget *parent, bool match_scale)
    : QScrollArea(parent),
      middleDown(false),
      m_match_scale(match_scale),
      lastPos() {
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
  if (m_match_scale) {
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
  } else
    return QScrollArea::event(e);
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

void EditorScrollArea::controlScroll(QScrollArea *scrollToControl,
                                     Qt::Orientation direction) {
  QScrollBar *bar;
  QScrollBar *barToControl;
  switch (direction) {
    case Qt::Horizontal:
      bar = horizontalScrollBar();
      barToControl = scrollToControl->horizontalScrollBar();
      break;
    case Qt::Vertical:
      bar = verticalScrollBar();
      barToControl = scrollToControl->verticalScrollBar();
      break;
  }
  connect(bar, &QAbstractSlider::valueChanged, [barToControl](int value) {
    if (barToControl->value() != value) barToControl->setValue(value);
  });
}

void EditorScrollArea::ensureWithinMargin(int x, qreal minDistFromLeft,
                                          qreal maxDistFromLeft) {
  minDistFromLeft *= viewport()->width();
  maxDistFromLeft *= viewport()->width();
  qreal slack = (maxDistFromLeft - minDistFromLeft) * 0.05;
  int logicalX =
      QStyle::visualPos(layoutDirection(), viewport()->rect(), QPoint(x, 0))
          .x();
  if (logicalX - minDistFromLeft < horizontalScrollBar()->value()) {
    horizontalScrollBar()->setValue(
        qMax(0, qint32(logicalX - maxDistFromLeft + slack)));
  } else if (logicalX - maxDistFromLeft > horizontalScrollBar()->value()) {
    horizontalScrollBar()->setValue(
        qMin(qint32(logicalX - minDistFromLeft - slack),
             horizontalScrollBar()->maximum()));
  }
}

// So keys like up / down work with the editor.
void EditorScrollArea::keyPressEvent(QKeyEvent *event) { event->ignore(); }
