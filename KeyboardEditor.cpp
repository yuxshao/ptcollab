#include "KeyboardEditor.h"

#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
#include <QTime>
int one_over_last_clock(pxtnService const *pxtn) {
  return pxtn->master->get_beat_clock() * (pxtn->master->get_meas_num() + 1) *
         pxtn->master->get_beat_num();
}

QSize KeyboardEditor::sizeHint() const {
  return QSize(one_over_last_clock(m_pxtn) / scale.clockPerPx,
               scale.pitchToY(EVENTMIN_KEY));
}
KeyboardEditor::KeyboardEditor(pxtnService *pxtn, QAudioOutput *audio_output,
                               QWidget *parent)
    : QWidget(parent),
      scale(),
      m_pxtn(pxtn),
      m_timer(new QElapsedTimer),
      painted(0),
      m_audio_output(audio_output),
      m_anim(new Animation(this)) {
  m_audio_output->setNotifyInterval(10);
  m_anim->setDuration(100);
  m_anim->setStartValue(0);
  m_anim->setEndValue(360);
  m_anim->setEasingCurve(QEasingCurve::Linear);
  m_anim->setLoopCount(-1);
  m_anim->start();
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  updateGeometry();

  connect(m_anim, SIGNAL(valueChanged(QVariant)), SLOT(update()));
  // connect(m_audio_output, SIGNAL(notify()), SLOT(update()));
}

struct Interval {
  int start;
  int end;

  bool contains(int x) const { return (start <= x && x < end); }

  int length() const { return end - start; }
};

struct LastEvent {
  int clock;
  int value;

  LastEvent(int value) : clock(0), value(value) {}

  void set(EVERECORD const *e) {
    clock = e->clock;
    value = e->value;
  }
};

struct DrawState {
  LastEvent pitch;
  LastEvent velocity;
  std::optional<Interval> ongoingOnEvent;

  DrawState()
      : pitch(EVENTDEFAULT_KEY),
        velocity(EVENTDEFAULT_VELOCITY),
        ongoingOnEvent(std::nullopt) {}
};

struct KeyBlock {
  int pitch;
  Interval segment;
  Interval onEvent;
};

constexpr int EVENTMAX_VELOCITY = 128;
static void paintBlock(int pitch, const Interval &segment, QPainter &painter,
                       const QBrush &brush, const Scale &scale) {
  painter.fillRect(segment.start / scale.clockPerPx, scale.pitchToY(pitch),
                   segment.length() / scale.clockPerPx,
                   PITCH_PER_KEY / scale.pitchPerPx - 2, brush);
}

static void paintVerticalLine(QPainter &painter, QBrush const &brush, int clock,
                              const Scale &scale) {
  painter.fillRect(clock / scale.clockPerPx, 0, 1, 10000, brush);
}

static int lerp(double r, int a, int b) {
  if (r > 1) r = 1;
  if (r < 0) r = 0;
  return a + r * (b - a);
}
static int clamp(int x, int lo, int hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}
struct Brush {
  int hue;
  int saturation;
  int muted_brightness;
  int base_brightness;
  int on_brightness;

  Brush(int hue, int saturation = 255, int muted_brightness = 20,
        int base_brightness = 204, int on_brightness = 255)
      : hue(hue),
        saturation(saturation),
        muted_brightness(muted_brightness),
        base_brightness(base_brightness),
        on_brightness(on_brightness) {}

  QBrush toQBrush(int velocity, bool on) {
    int brightness =
        lerp(double(velocity) / EVENTMAX_VELOCITY, muted_brightness,
             on ? on_brightness : base_brightness);
    return QBrush(QColor::fromHsl(hue, saturation, brightness));
  }
};

int pixelsPerVelocity = 3;
int impliedVelocity(MouseEditState state, const Scale &scale) {
  return clamp(
      EVENTDEFAULT_VELOCITY + (state.current_pitch - state.start_pitch) /
                                  scale.pitchPerPx / pixelsPerVelocity,
      0, EVENTMAX_VELOCITY);
}
// TODO: Make an FPS tracker singleton
static qreal iFps;
void KeyboardEditor::paintEvent(QPaintEvent *) {
  ++painted;
  // if (painted > 10) return;
  QPainter painter(this);

  // Draw FPS
  {
    int interval = 20;
    if (!(painted % interval)) {
      qint64 elapsed = m_timer->nsecsElapsed();
      m_timer->restart();
      iFps = 1E9 / elapsed * interval;
    }
    painter.drawText(rect(), QString("%1 FPS").arg(iFps, 0, 'f', 0));
  }

  // Set up drawing structures that we'll use while iterating through events
  std::vector<DrawState> drawStates;
  std::vector<Brush> brushes;
  for (int i = 0; i < m_pxtn->Unit_Num(); ++i) {
    drawStates.emplace_back();
    brushes.emplace_back((360 * i * 3 / 7) % 360, 255);
  }
  painter.setPen(Qt::blue);
  // TODO: usecs is choppy - it's an upper bound that gets worse with buffer
  // size incrase. for longer songs though and lower end comps we probably do
  // want a bigger buffer. The formula fixes the upper bound issue, but perhaps
  // we can do some smoothing with a linear thing too.
  // int bytes_per_second = 4 /* bytes in sample */ * 44100 /* samples per
  // second */; long usecs = m_audio_output->processedUSecs() -
  // long(m_audio_output->bufferSize()) * 10E5 / bytes_per_second;

  long usecs = m_audio_output->processedUSecs();
  int clock = usecs * m_pxtn->master->get_beat_tempo() *
              m_pxtn->master->get_beat_clock() / 60 / 1000000;
  // clock = m_pxtn->moo_get_now_clock();

  int repeat_clock = m_pxtn->master->get_repeat_meas() *
                     m_pxtn->master->get_beat_num() *
                     m_pxtn->master->get_beat_clock();
  int last_clock = m_pxtn->master->get_beat_clock() *
                   m_pxtn->master->get_play_meas() *
                   m_pxtn->master->get_beat_num();
  clock = (clock - repeat_clock) % last_clock + repeat_clock;

  // Draw the note blocks! Upon hitting an event, see if we are able to draw a
  // previous block.
  for (const EVERECORD *e = m_pxtn->evels->get_Records(); e != nullptr;
       e = e->next) {
    int i = e->unit_no;
    switch (e->kind) {
      case EVENTKIND_ON:
        // Draw the last block of the previous on event if there's one to draw.
        // TODO: This 'draw previous note block' is duplicated quite a bit.
        if (drawStates[i].ongoingOnEvent.has_value()) {
          Interval on = drawStates[i].ongoingOnEvent.value();
          int start = std::max(drawStates[i].pitch.clock, on.start);
          int end = std::min(e->clock, on.end);
          Interval interval{start, end};
          QBrush brush = brushes[i].toQBrush(drawStates[i].velocity.value,
                                             on.contains(clock));
          paintBlock(drawStates[i].pitch.value, interval, painter, brush,
                     scale);
          if (start == on.start)
            paintBlock(drawStates[i].pitch.value,
                       {start, start + int(2 * scale.clockPerPx)}, painter,
                       brushes[i].toQBrush(255, true), scale);
        }
        drawStates[i].ongoingOnEvent.emplace(
            Interval{e->clock, e->value + e->clock});
        break;
      case EVENTKIND_VELOCITY:
        drawStates[i].velocity.set(e);
        break;
      case EVENTKIND_KEY:
        // Maybe draw the previous segment of the current on event.
        if (drawStates[i].ongoingOnEvent.has_value()) {
          Interval on = drawStates[i].ongoingOnEvent.value();
          int start = std::max(drawStates[i].pitch.clock, on.start);
          int end = std::min(e->clock, on.end);
          Interval interval{start, end};
          QBrush brush = brushes[i].toQBrush(drawStates[i].velocity.value,
                                             on.contains(clock));
          paintBlock(drawStates[i].pitch.value, interval, painter, brush,
                     scale);
          if (start == on.start)
            paintBlock(drawStates[i].pitch.value,
                       {start, start + 2 * int(scale.clockPerPx)}, painter,
                       brushes[i].toQBrush(255, true), scale);
          if (e->clock > on.end) drawStates[i].ongoingOnEvent.reset();
        }
        drawStates[i].pitch.set(e);
        break;
      default:
        break;
    }
  }

  // After all the events there might be some blocks that are pending a draw.
  for (uint i = 0; i < drawStates.size(); ++i) {
    if (drawStates[i].ongoingOnEvent.has_value()) {
      Interval on = drawStates[i].ongoingOnEvent.value();
      int start = std::max(drawStates[i].pitch.clock, on.start);
      Interval interval{start, on.end};
      QBrush brush =
          brushes[i].toQBrush(drawStates[i].velocity.value, on.contains(clock));
      paintBlock(drawStates[i].pitch.value, interval, painter, brush, scale);
      if (start == on.start)
        paintBlock(drawStates[i].pitch.value,
                   {start, start + 2 * int(scale.clockPerPx)}, painter,
                   brushes[i].toQBrush(255, true), scale);
      drawStates[i].ongoingOnEvent.reset();
    }
  }

  // Draw an ongoing edit
  if (m_mouse_edit_state != nullptr) {
    int velocity = impliedVelocity(*m_mouse_edit_state, scale);
    Interval interval{m_mouse_edit_state->start_clock,
                      m_mouse_edit_state->current_clock};
    paintBlock(m_mouse_edit_state->start_pitch, interval, painter,
               brushes[0].toQBrush(velocity, false), scale);
    painter.drawText(m_mouse_edit_state->start_clock / scale.clockPerPx,
                     scale.pitchToY(m_mouse_edit_state->start_pitch),
                     QString("(%1, %2, %3, %4)")
                         .arg(m_mouse_edit_state->start_clock)
                         .arg(m_mouse_edit_state->current_clock)
                         .arg(m_mouse_edit_state->start_pitch)
                         .arg(velocity));
  }

  // clock = us * 1s/10^6us * 1m/60s * tempo beats/m * beat_clock clock/beats
  paintVerticalLine(painter, QBrush(QColor::fromRgb(255, 255, 255)), clock,
                    scale);
  paintVerticalLine(painter, QBrush(QColor::fromRgb(255, 255, 255, 128)),
                    last_clock, scale);
  paintVerticalLine(painter, QBrush(QColor::fromRgb(255, 255, 255, 128)),
                    m_pxtn->moo_get_end_clock(), scale);
}

void KeyboardEditor::mousePressEvent(QMouseEvent *event) {
  int clock = event->localPos().x() * scale.clockPerPx;
  int pitch = int(round(scale.pitchOfY(event->localPos().y())));
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) return;
  MouseEditState::Type type;
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->button() == Qt::RightButton)
      type = MouseEditState::Type::DeleteNote;
    else
      type = MouseEditState::Type::SetNote;
  } else {
    if (event->button() == Qt::RightButton)
      type = MouseEditState::Type::DeleteOn;
    else
      type = MouseEditState::Type::SetOn;
  }

  m_mouse_edit_state.reset(
      new MouseEditState{type, clock, pitch, clock, pitch});
}

void KeyboardEditor::wheelEvent(QWheelEvent *event) {
  QPoint delta = event->angleDelta();
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->modifiers() & Qt::ShiftModifier) {
      // scale Y
      scale.pitchPerPx *= pow(2, delta.y() / 240.0);
      if (scale.pitchPerPx < 0.5) scale.pitchPerPx = 0.5;
    } else {
      // scale X
      scale.clockPerPx *= pow(2, delta.y() / 240.0);
      if (scale.clockPerPx < 0.5) scale.clockPerPx = 0.5;
    }

    updateGeometry();
    event->accept();
  }
}

void KeyboardEditor::mouseMoveEvent(QMouseEvent *event) {
  if (m_mouse_edit_state == nullptr) return;
  // TODO: Perhaps the edit state should just work in pixel coords. Enough
  // places don't really care about the coord in pitch / clock scale.
  m_mouse_edit_state->current_pitch =
      int(round(scale.pitchOfY(event->localPos().y())));
  m_mouse_edit_state->current_clock = event->localPos().x() * scale.clockPerPx;
}

void KeyboardEditor::mouseReleaseEvent(QMouseEvent *event) {
  if (m_mouse_edit_state == nullptr) return;

  int start_clock = m_mouse_edit_state->start_clock;
  int end_clock = event->localPos().x() * scale.clockPerPx;
  if (end_clock < start_clock) std::swap(start_clock, end_clock);

  int start_pitch = m_mouse_edit_state->start_pitch;
  // int end_pitch = int(round(pitchOfY(event->localPos().y())));
  int start_measure = start_clock / m_pxtn->master->get_beat_clock() /
                      m_pxtn->master->get_beat_num();
  int end_measure = end_clock / m_pxtn->master->get_beat_clock() /
                    m_pxtn->master->get_beat_num();

  switch (m_mouse_edit_state->type) {
    case MouseEditState::SetOn:
      m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_ON);
      m_pxtn->evels->Record_Delete(start_clock, end_clock, 0,
                                   EVENTKIND_VELOCITY);
      m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_KEY);
      m_pxtn->evels->Record_Add_i(start_clock, 0, EVENTKIND_ON,
                                  end_clock - start_clock);
      m_pxtn->evels->Record_Add_i(start_clock, 0, EVENTKIND_VELOCITY,
                                  impliedVelocity(*m_mouse_edit_state, scale));
      m_pxtn->evels->Record_Add_i(start_clock, 0, EVENTKIND_KEY, start_pitch);
      if (end_measure >= m_pxtn->master->get_meas_num()) {
        m_pxtn->master->set_meas_num(end_measure + 1);
        updateGeometry();
      }
      break;
    case MouseEditState::DeleteOn:
      m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_ON);
      m_pxtn->evels->Record_Delete(start_clock, end_clock, 0,
                                   EVENTKIND_VELOCITY);
      break;
    case MouseEditState::SetNote:
      m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_KEY);
      m_pxtn->evels->Record_Add_i(start_clock, 0, EVENTKIND_KEY, start_pitch);
      if (start_measure >= m_pxtn->master->get_meas_num())
        m_pxtn->master->set_meas_num(start_measure + 1);
      break;
    case MouseEditState::DeleteNote:
      m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_KEY);
      break;
  }
  m_mouse_edit_state.reset();
}
