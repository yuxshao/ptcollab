#include "LeftPianoView.h"

#include <QPaintEvent>

#include "ViewHelper.h"
#include "editor/Settings.h"
#include "editor/StyleEditor.h"

LeftPianoView::LeftPianoView(PxtoneClient *client, MooClock *moo_clock,
                             QWidget *parent)
    : QWidget(parent),
      m_anim(new Animation(this)),
      m_client(client),
      m_moo_clock(moo_clock) {
  connect(m_anim, &Animation::nextFrame, [this]() { update(); });
  connect(m_client, &PxtoneClient::editStateChanged,
          [this](const EditState &s) {
            if (!(m_last_scale == s.scale)) {
              updateGeometry();
              m_last_scale = s.scale;
            }
          });
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

void LeftPianoView::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.fillRect(0, 0, size().width(), size().height(), Qt::black);
  QColor rootNoteBrush = StyleEditor::config.color.KeyboardRootNote;
  QColor whiteNoteBrush = StyleEditor::config.color.KeyboardWhiteNote;
  QColor blackNoteBrush = StyleEditor::config.color.KeyboardBlackNote;

  QColor whiteLeftBrush = StyleEditor::config.color.KeyboardWhiteLeft;
  QColor blackLeftBrush = StyleEditor::config.color.KeyboardBlackLeft;

  QLinearGradient gradient(0, 0, 1, 0);
  gradient.setColorAt(0.5, rootNoteBrush);
  gradient.setColorAt(1, Qt::transparent);
  gradient.setCoordinateMode(QGradient::ObjectMode);
  double pitchPerPx = m_client->editState().scale.pitchPerPx;
  const QList<int> &displayEdoList = Settings::DisplayEdo::get();
  int displayEdo = displayEdoList.size();
  const Scale &scale = m_client->editState().scale;
  bool octave_display_a = Settings::OctaveDisplayA::get();

  QPixmap octaveDisplayLayer(event->rect().size());
  octaveDisplayLayer.fill(Qt::transparent);
  QPainter octaveDisplayPainter(&octaveDisplayLayer);
  octaveDisplayPainter.translate(-event->rect().topLeft());
  for (int row = 0; true; ++row) {
    QColor *brush, *leftBrush;
    // Start the backgrounds on an A just so that the key pattern lines up
    int a_above_max_key =
        (EVENTMAX_KEY / PITCH_PER_OCTAVE + 1) * PITCH_PER_OCTAVE;
    int pitch = quantize_pitch(
        a_above_max_key - row * PITCH_PER_OCTAVE / displayEdo, displayEdo);
    int nextPitch =
        quantize_pitch(pitch - PITCH_PER_OCTAVE / displayEdo, displayEdo);
    if (pitch == EVENTDEFAULT_KEY) {
      brush = &rootNoteBrush;
      leftBrush = &whiteLeftBrush;
    } else {
      if (displayEdoList[nonnegative_modulo(-row, displayEdo)]) {
        brush = &blackNoteBrush;
        leftBrush = &blackLeftBrush;
      } else {
        brush = &whiteNoteBrush;
        leftBrush = &whiteLeftBrush;
      }
    }

    int floor_h = PITCH_PER_OCTAVE / pitchPerPx / displayEdo;
    int this_y = scale.pitchToY(pitch);
    int next_y = scale.pitchToY(nextPitch);

    if (this_y > size().height()) break;
    // Because of rounding error, calculate height by subbing next from this
    int h = next_y - this_y - 1;
    painter.fillRect(0, this_y - floor_h / 2, size().width(), h, *brush);

    drawLeftPiano(painter, -pos().x(), this_y - floor_h / 2, h, *leftBrush);
    // painter.fillRect(0, this_y, 9999, 1, QColor::fromRgb(255, 255, 255,
    // 50));

    int pitch_offset = 0;
    if (!octave_display_a) pitch_offset = PITCH_PER_OCTAVE / 4;
    if (nonnegative_modulo(pitch - EVENTDEFAULT_KEY, PITCH_PER_OCTAVE) ==
        pitch_offset)
      drawOctaveNumAlignBottomLeft(
          &octaveDisplayPainter, -pos().x() + 4, this_y - floor_h / 2 + h - 2,
          (pitch - PITCH_PER_OCTAVE / 4) / PITCH_PER_OCTAVE - 3, floor_h,
          octave_display_a);
  }
  painter.drawPixmap(event->rect(), octaveDisplayLayer,
                     octaveDisplayLayer.rect());
}

void LeftPianoView::wheelEvent(QWheelEvent *event) {
  handleWheelEventWithModifier(event, m_client);
}
QSize LeftPianoView::sizeHint() const {
  return QSize(LEFT_LEGEND_WIDTH,
               m_client->editState().scale.pitchToY(EVENTMIN_KEY));
}
