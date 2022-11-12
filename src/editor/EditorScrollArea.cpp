#include "EditorScrollArea.h"

#include <QApplication>
#include <QDebug>
#include <QScrollBar>
#include <QStyle>

#include "Settings.h"

class TransposableWheelEvent : QWheelEvent {
 public:
  void transpose() {
    // not using QPoint::transposed() because it doesn't exist in older versions
    pixelD = {pixelD.y(), pixelD.x()};
    angleD = {angleD.y(), angleD.x()};
  }
};

QRect EditorScrollArea::viewportRect() {
  return QRect(this->horizontalScrollBar()->value(),
               this->verticalScrollBar()->value(), viewport()->width(),
               viewport()->height());
}

EditorScrollArea::EditorScrollArea(QWidget *parent, bool match_scale)
    : QScrollArea(parent),
      mouseDown(false),
      m_match_scale(match_scale),
      lastPos(),
      anim(new Animation(this)),
      m_scroll_with_mouse_x(true) {
  setWidgetResizable(true);
  setMouseTracking(true);
  setFrameStyle(QFrame::NoFrame);
  for (const auto &bar : {horizontalScrollBar(), verticalScrollBar()}) {
    connect(bar, &QAbstractSlider::valueChanged,
            [this]() { emit viewportChanged(viewportRect()); });
  };
  connect(anim, &Animation::nextFrame, [this]() {
    // This is a bit of a hack so that on release we can immediately stop
    // scrolling.
    if (!(QApplication::mouseButtons() & (Qt::LeftButton | Qt::RightButton)))
      mouseDown = false;
    if (mouseDown && m_scroll_with_mouse_x) scrollWithMouseX();
  });
}

void EditorScrollArea::mousePressEvent(QMouseEvent *event) {
  // TODO: In reality this mouseDown is never triggered because the child eats
  // the event. Similarly in release.
  mouseDown =
      event->buttons() & Qt::LeftButton || event->buttons() & Qt::RightButton;
  // double ratioH = double(lastPos.x()) / viewport()->width();

  /*qDebug() << horizontalScrollBar()->pageStep()
           << horizontalScrollBar()->value() << horizontalScrollBar()->maximum()
           << ratioH;*/
  if (event->button() & Qt::MiddleButton) {
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

void EditorScrollArea::updateMouseDownState(QMouseEvent *e) {
  mouseDown = (e->buttons() & Qt::LeftButton || e->buttons() & Qt::RightButton);
}

void EditorScrollArea::mouseReleaseEvent(QMouseEvent *event) {
  event->ignore();
  updateMouseDownState(event);
  if (event->button() & Qt::MiddleButton) {
    viewport()->unsetCursor();
    event->accept();
  }
  lastPos = event->pos();
}

void EditorScrollArea::mouseMoveEvent(QMouseEvent *event) {
  event->ignore();
  updateMouseDownState(event);
  if (event->buttons() & Qt::MiddleButton) {
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
  // Maybe scroll the other dimension. != is xor
  bool shift = event->modifiers() & Qt::ShiftModifier;
  bool swap_horizontal = (event->modifiers() & Qt::ControlModifier
                              ? Settings::SwapZoomOrientation::get()
                              : Settings::SwapScrollOrientation::get());
  if (shift != swap_horizontal) ((TransposableWheelEvent *)event)->transpose();

  // Disable modifier because QScrollArea takes it to mean scroll by a page
  // QT by default has alt scroll horizontally, but pxtone uses shift.
  event->setModifiers(event->modifiers() & ~Qt::AltModifier &
                      ~Qt::ShiftModifier);

  QScrollArea::wheelEvent(event);
}

void EditorScrollArea::resizeEvent(QResizeEvent *event) {
  QScrollArea::resizeEvent(event);
  emit viewportChanged(viewportRect());
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
    default:
      bar = verticalScrollBar();
      barToControl = scrollToControl->verticalScrollBar();
      break;
  }
  connect(bar, &QAbstractSlider::valueChanged, [bar, barToControl](int value) {
    if (barToControl->value() != value) {
      // setMaximum/setMinimum is a hack - when there are two synced bars that
      // respond to the same resize event, one of them will change their range
      // first. Setting the other's value to the first one will cause a jump if
      // the value is out of the second's range, so force the ranges to be the
      // same here.
      barToControl->setMinimum(bar->minimum());
      barToControl->setMaximum(bar->maximum());
      barToControl->setValue(value);
    }
  });
}

void EditorScrollArea::ensureWithinMargin(int x, qreal minDistFromLeft,
                                          qreal jumpMinDistFromLeft,
                                          qreal jumpMaxDistFromLeft,
                                          qreal maxDistFromLeft) {
  minDistFromLeft *= viewport()->width();
  maxDistFromLeft *= viewport()->width();
  jumpMaxDistFromLeft *= viewport()->width();
  jumpMinDistFromLeft *= viewport()->width();
  int logicalX =
      QStyle::visualPos(layoutDirection(), viewport()->rect(), QPoint(x, 0))
          .x();
  if (logicalX - minDistFromLeft < horizontalScrollBar()->value()) {
    horizontalScrollBar()->setValue(
        std::max(0, qint32(logicalX - jumpMaxDistFromLeft)));
  } else if (logicalX - maxDistFromLeft > horizontalScrollBar()->value()) {
    horizontalScrollBar()->setValue(
        std::min(qint32(logicalX - jumpMinDistFromLeft),
                 horizontalScrollBar()->maximum()));
  }
}

void EditorScrollArea::setEnableScrollWithMouseX(bool b) {
  m_scroll_with_mouse_x = b;
}

constexpr int JUMP_MAX = 10;
constexpr int MARGIN_MAX = 100;
constexpr double MARGIN_FRAC = 0.25;
void EditorScrollArea::scrollWithMouseX() {
  int logicalX = QStyle::visualPos(layoutDirection(), viewport()->rect(),
                                   mapFromGlobal(QCursor::pos()))
                     .x();
  int margin = std::min(MARGIN_MAX, int(viewport()->width() * MARGIN_FRAC));
  double scroll = 0;
  if (logicalX < margin) scroll = (logicalX - margin + 0.0) / margin;
  if (logicalX > viewport()->width() - margin)
    scroll = (logicalX - viewport()->width() + margin + 0.0) / margin;

  if (scroll != 0) {
    scroll *= abs(scroll);
    scroll = std::clamp(scroll, -1.5, 1.5);
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() +
                                    JUMP_MAX * scroll);
  }
}

// So keys like up / down work with the editor.
void EditorScrollArea::keyPressEvent(QKeyEvent *event) { event->ignore(); }
