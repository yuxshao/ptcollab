#include "KeyboardEditor.h"

#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollArea>
#include <QTime>

#include "PxtoneUnitIODevice.h"

int quantize(int v, int q) { return (v / q) * q; }
int one_over_last_clock(pxtnService const *pxtn) {
  return pxtn->master->get_beat_clock() * (pxtn->master->get_meas_num() + 1) *
         pxtn->master->get_beat_num();
}

QSize KeyboardEditor::sizeHint() const {
  return QSize(one_over_last_clock(m_pxtn) / scale.clockPerPx,
               scale.pitchToY(EVENTMIN_KEY));
}
KeyboardEditor::KeyboardEditor(pxtnService *pxtn, QAudioOutput *audio_output,
                               QScrollArea *parent)
    : QWidget(parent),
      scale(),
      m_pxtn(pxtn),
      m_timer(new QElapsedTimer),
      m_current_unit(0),
      m_show_all_units(false),
      painted(0),
      m_mouse_edit_state{MouseEditState::Type::Nothing, 0, 0, 0, 0, nullptr},
      m_audio_output(audio_output),
      m_anim(new Animation(this)),
      m_quantize_clock(pxtn->master->get_beat_clock()),
      m_quantize_pitch(PITCH_PER_KEY) {
  m_audio_output->setNotifyInterval(10);
  m_anim->setDuration(100);
  m_anim->setStartValue(0);
  m_anim->setEndValue(360);
  m_anim->setEasingCurve(QEasingCurve::Linear);
  m_anim->setLoopCount(-1);
  m_anim->start();
  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  updateGeometry();
  setMouseTracking(true);

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
static void paintAtClockPitch(int clock, int pitch, int widthInPx,
                              QPainter &painter, const QBrush &brush,
                              const Scale &scale) {
  int rowHeight = PITCH_PER_KEY / scale.pitchPerPx;
  painter.fillRect(clock / scale.clockPerPx,
                   scale.pitchToY(pitch) + rowHeight / 6, widthInPx,
                   rowHeight * 2 / 3, brush);
}

static void paintBlock(int pitch, const Interval &segment, QPainter &painter,
                       const QBrush &brush, const Scale &scale) {
  paintAtClockPitch(segment.start, pitch, segment.length() / scale.clockPerPx,
                    painter, brush, scale);
}

static void paintHighlight(int pitch, int clock, QPainter &painter,
                           const QBrush &brush, const Scale &scale) {
  paintAtClockPitch(clock, pitch, 2, painter, brush, scale);
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
  int muted_saturation;
  int base_saturation;
  int muted_brightness;
  int base_brightness;
  int on_brightness;

  Brush(int hue, int muted_saturation = 48, int base_saturation = 255,
        int muted_brightness = 96, int base_brightness = 204,
        int on_brightness = 255)
      : hue(hue),
        muted_saturation(muted_saturation),
        base_saturation(base_saturation),
        muted_brightness(muted_brightness),
        base_brightness(base_brightness),
        on_brightness(on_brightness) {}

  QBrush toQBrush(int velocity, bool on, int alpha) {
    int brightness =
        lerp(double(velocity) / EVENTMAX_VELOCITY, muted_brightness,
             on ? on_brightness : base_brightness);
    int saturation = lerp(double(velocity) / EVENTMAX_VELOCITY,
                          muted_saturation, base_saturation);
    return QBrush(QColor::fromHsl(hue, saturation, brightness, alpha));
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

  painter.fillRect(0, 0, size().width(), size().height(), Qt::black);
  // Draw white lines under background
  QBrush beatBrush(QColor::fromRgb(128, 128, 128));
  QBrush measureBrush(Qt::white);
  for (int beat = 0; true; ++beat) {
    bool isMeasureLine = (beat % m_pxtn->master->get_beat_num() == 0);
    int x = m_pxtn->master->get_beat_clock() * beat / scale.clockPerPx;
    if (x > size().width()) break;
    painter.fillRect(x, 0, 1, size().height(),
                     (isMeasureLine ? measureBrush : beatBrush));
  }
  // Draw key background
  QBrush whiteNoteBrush(QColor::fromRgb(64, 64, 64));
  QBrush blackNoteBrush(QColor::fromRgb(32, 32, 32));
  for (int row = 0; true; ++row) {
    QBrush *brush;
    switch (row % 12) {
      case 2:
      case 4:
      case 6:
      case 9:
      case 11:
        brush = &blackNoteBrush;
        break;
      default:
        brush = &whiteNoteBrush;
    }

    if (row * PITCH_PER_KEY / scale.pitchPerPx > size().height()) break;
    int this_y = row * PITCH_PER_KEY / scale.pitchPerPx;
    // Because of rounding error, calculate height by subbing next from this
    // TODO: change scale so that pitchPerPx is necessarily an int
    int next_y = (row + 1) * PITCH_PER_KEY / scale.pitchPerPx;
    painter.fillRect(0, this_y, size().width(), next_y - this_y - 1, *brush);
  }

  // Draw FPS
  QPen pen;
  pen.setBrush(Qt::white);
  painter.setPen(pen);
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
    brushes.emplace_back((360 * i * 3 / 7) % 360);
  }
  painter.setPen(Qt::blue);
  // TODO: usecs is choppy - it's an upper bound that gets worse with buffer
  // size incrase. for longer songs though and lower end comps we probably do
  // want a bigger buffer. The formula fixes the upper bound issue, but perhaps
  // we can do some smoothing with a linear thing too.
  // int bytes_per_second = 4 /* bytes in sample */ * 44100 /* samples per
  // second */; long usecs = m_audio_output->processedUSecs() -
  // long(m_audio_output->bufferSize()) * 10E5 / bytes_per_second;

  /*long usecs = m_audio_output->processedUSecs();
  int clock = usecs * m_pxtn->master->get_beat_tempo() *
              m_pxtn->master->get_beat_clock() / 60 / 1000000;*/
  int clock = m_pxtn->moo_get_now_clock();

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
    int alpha = (i == m_current_unit || m_show_all_units ? 255 : 64);
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
                                             on.contains(clock), alpha);
          paintBlock(drawStates[i].pitch.value, interval, painter, brush,
                     scale);
          if (start == on.start)
            paintHighlight(drawStates[i].pitch.value, start, painter,
                           brushes[i].toQBrush(255, true, alpha), scale);
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
                                             on.contains(clock), alpha);
          paintBlock(drawStates[i].pitch.value, interval, painter, brush,
                     scale);
          if (start == on.start)
            paintHighlight(drawStates[i].pitch.value, start, painter,
                           brushes[i].toQBrush(255, true, alpha), scale);
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
      int alpha = (int(i) == m_current_unit || m_show_all_units ? 255 : 64);
      Interval on = drawStates[i].ongoingOnEvent.value();
      int start = std::max(drawStates[i].pitch.clock, on.start);
      Interval interval{start, on.end};
      QBrush brush = brushes[i].toQBrush(drawStates[i].velocity.value,
                                         on.contains(clock), alpha);
      paintBlock(drawStates[i].pitch.value, interval, painter, brush, scale);
      if (start == on.start)
        paintBlock(drawStates[i].pitch.value,
                   {start, start + 2 * int(scale.clockPerPx)}, painter,
                   brushes[i].toQBrush(255, true, alpha), scale);
      drawStates[i].ongoingOnEvent.reset();
    }
  }

  QBrush halfWhite(QColor::fromRgb(255, 255, 255, 128));
  // Draw an ongoing edit or seek
  if (uint(m_current_unit) < brushes.size()) {  // In case nothing's loaded
    switch (m_mouse_edit_state.type) {
      case MouseEditState::Type::Nothing:
      case MouseEditState::Type::SetOn:
      case MouseEditState::Type::DeleteOn:
      case MouseEditState::Type::SetNote:
      case MouseEditState::Type::DeleteNote: {
        int velocity = impliedVelocity(m_mouse_edit_state, scale);
        // TODO: maybe factor out this quantization logic
        Interval interval{
            quantize(m_mouse_edit_state.start_clock, m_quantize_clock),
            quantize(m_mouse_edit_state.current_clock, m_quantize_clock) +
                m_quantize_clock};
        int pitch = quantize(m_mouse_edit_state.start_pitch, m_quantize_pitch);
        int alpha =
            (m_mouse_edit_state.type == MouseEditState::Nothing ? 128 : 255);
        paintBlock(pitch, interval, painter,
                   brushes[m_current_unit].toQBrush(velocity, false, alpha),
                   scale);

        paintHighlight(pitch, std::min(interval.start, interval.end), painter,
                       brushes[m_current_unit].toQBrush(255, true, alpha),
                       scale);

        if (m_mouse_edit_state.type != MouseEditState::Nothing) {
          painter.setPen(pen);
          painter.drawText(interval.start / scale.clockPerPx,
                           scale.pitchToY(pitch),
                           QString("(%1, %2, %3, %4)")
                               .arg(interval.start)
                               .arg(interval.end)
                               .arg(pitch)
                               .arg(velocity));
        }
      } break;
      case MouseEditState::Type::Seek:
        painter.fillRect(m_mouse_edit_state.current_clock / scale.clockPerPx, 0,
                         1, size().height(), halfWhite);
        break;
    }
  }

  // clock = us * 1s/10^6us * 1m/60s * tempo beats/m * beat_clock clock/beats
  painter.fillRect(clock / scale.clockPerPx, 0, 1, size().height(), Qt::white);
  painter.fillRect(last_clock / scale.clockPerPx, 0, 1, size().height(),
                   halfWhite);
  painter.fillRect(m_pxtn->moo_get_end_clock() / scale.clockPerPx, 0, 1,
                   size().height(), halfWhite);
}

void KeyboardEditor::wheelEvent(QWheelEvent *event) {
  // TODO: Unfortunately, it's hard to when scaling preserve the time/pitch
  // position of the mouse or playhead or whatever else. There isn't a really
  // good hook for controlling the scroll bar positions - the fact that they're
  // in the parent makes this harder too.
  QPoint delta = event->angleDelta();
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->modifiers() & Qt::ShiftModifier) {
      // scale Y
      scale.pitchPerPx *= pow(2, delta.y() / 240.0);
      if (scale.pitchPerPx < 8) scale.pitchPerPx = 8;
      if (scale.pitchPerPx > PITCH_PER_KEY / 8)
        scale.pitchPerPx = PITCH_PER_KEY / 8;
    } else {
      // scale X
      scale.clockPerPx *= pow(2, delta.y() / 240.0);
      if (scale.clockPerPx < 0.5) scale.clockPerPx = 0.5;
      if (scale.clockPerPx > 128) scale.clockPerPx = 128;
    }

    updateGeometry();
    event->accept();
  }
}

QAudioOutput *KeyboardEditor::make_audio(int pitch) {
  // TODO: Deduplicate this sample setup
  QAudioFormat format;
  int channel_num = 2;
  int sample_rate = 44100;
  format.setSampleRate(sample_rate);
  format.setChannelCount(channel_num);
  format.setSampleSize(16);
  format.setCodec("audio/pcm");
  format.setByteOrder(QAudioFormat::LittleEndian);
  format.setSampleType(QAudioFormat::SignedInt);
  QAudioOutput *audio = new QAudioOutput(format, this);
  PxtoneUnitIODevice *m_pxtn_device =
      new PxtoneUnitIODevice(audio, m_pxtn, m_current_unit, pitch);
  m_pxtn_device->open(QIODevice::ReadOnly);

  audio->setVolume(1.0);
  audio->start(m_pxtn_device);
  return audio;
}
void KeyboardEditor::mousePressEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }
  if (m_pxtn->Unit_Num() == 0) return;
  int clock = event->localPos().x() * scale.clockPerPx;
  int pitch = scale.pitchOfY(event->localPos().y());
  QAudioOutput *audio = nullptr;
  MouseEditState::Type type;
  if (event->modifiers() & Qt::ShiftModifier) {
    type = MouseEditState::Type::Seek;
  } else {
    if (event->modifiers() & Qt::ControlModifier) {
      if (event->button() == Qt::RightButton)
        type = MouseEditState::Type::DeleteNote;
      else {
        type = MouseEditState::Type::SetNote;
        audio = make_audio(quantize(pitch, m_quantize_pitch));
      }
    } else {
      if (event->button() == Qt::RightButton)
        type = MouseEditState::Type::DeleteOn;
      else {
        type = MouseEditState::Type::SetOn;
        audio = make_audio(pitch);
      }
    }
  }
  m_mouse_edit_state = MouseEditState{type, clock, pitch, clock, pitch, audio};
}

void KeyboardEditor::mouseMoveEvent(QMouseEvent *event) {
  if (m_mouse_edit_state.type == MouseEditState::Type::Nothing &&
      event->modifiers() & Qt::ShiftModifier) {
    m_mouse_edit_state.type = MouseEditState::Type::Seek;
  }
  if (m_mouse_edit_state.type == MouseEditState::Type::Seek &&
      !(event->modifiers() & Qt::ShiftModifier)) {
    m_mouse_edit_state.type = MouseEditState::Type::Nothing;
  }
  // TODO: The edit state should just work in pixel coords. Enough
  // places don't really care about the coord in pitch / clock scale.
  // Update: Okay maybe not because quantize is useful. Maybe we need more
  // heavyweight abstr

  m_mouse_edit_state.current_clock = event->localPos().x() * scale.clockPerPx;
  m_mouse_edit_state.current_pitch = scale.pitchOfY(event->localPos().y());
  if (m_mouse_edit_state.type == MouseEditState::Type::Nothing) {
    m_mouse_edit_state.start_clock = m_mouse_edit_state.current_clock;
    m_mouse_edit_state.start_pitch = m_mouse_edit_state.current_pitch;
  }
  event->ignore();
}

int nonnegative_modulo(int x, int m) {
  if (m == 0) return 0;
  return ((x % m) + m) % m;
}
void KeyboardEditor::cycleCurrentUnit(int offset) {
  switch (m_mouse_edit_state.type) {
    case MouseEditState::Type::SetOn:
    case MouseEditState::Type::DeleteOn:
    case MouseEditState::Type::SetNote:
    case MouseEditState::Type::DeleteNote:
      break;
    case MouseEditState::Type::Nothing:
    case MouseEditState::Type::Seek:
      m_current_unit =
          nonnegative_modulo(m_current_unit + offset, m_pxtn->Unit_Num());
      emit currentUnitChanged(m_current_unit);
      break;
  }
}

void KeyboardEditor::setCurrentUnit(int unit) { m_current_unit = unit; }

void KeyboardEditor::toggleShowAllUnits() {
  m_show_all_units = !m_show_all_units;
  emit showAllChanged(m_show_all_units);
}

void KeyboardEditor::setQuantX(int q) {
  m_quantize_clock = m_pxtn->master->get_beat_clock() / q;
}
void KeyboardEditor::setQuantY(int q) {
  qDebug() << q;
  m_quantize_pitch = PITCH_PER_KEY / q;
}

void KeyboardEditor::mouseReleaseEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }
  if (m_mouse_edit_state.audio) {
    m_mouse_edit_state.audio->suspend();
    m_mouse_edit_state.audio->deleteLater();
    m_mouse_edit_state.audio = nullptr;
  }
  Interval interval{
      quantize(m_mouse_edit_state.start_clock, m_quantize_clock),
      quantize(m_mouse_edit_state.current_clock, m_quantize_clock) +
          m_quantize_clock};
  Interval clock_int{std::min(interval.start, interval.end),
                     std::max(interval.start, interval.end)};
  int start_pitch = quantize(m_mouse_edit_state.start_pitch, m_quantize_pitch);
  // int end_pitch = int(round(pitchOfY(event->localPos().y())));
  int clockPerMeas =
      m_pxtn->master->get_beat_clock() * m_pxtn->master->get_beat_num();
  Interval meas_int{clock_int.start / clockPerMeas,
                    clock_int.end / clockPerMeas};

  if (m_pxtn->Unit_Num() > 0) switch (m_mouse_edit_state.type) {
      case MouseEditState::SetOn:
        m_pxtn->evels->Record_Delete(clock_int.start, clock_int.end,
                                     m_current_unit, EVENTKIND_ON);
        m_pxtn->evels->Record_Delete(clock_int.start, clock_int.end,
                                     m_current_unit, EVENTKIND_VELOCITY);
        m_pxtn->evels->Record_Delete(clock_int.start, clock_int.end,
                                     m_current_unit, EVENTKIND_KEY);
        m_pxtn->evels->Record_Add_i(clock_int.start, m_current_unit,
                                    EVENTKIND_ON, clock_int.length());
        m_pxtn->evels->Record_Add_i(clock_int.start, m_current_unit,
                                    EVENTKIND_VELOCITY,
                                    impliedVelocity(m_mouse_edit_state, scale));
        m_pxtn->evels->Record_Add_i(clock_int.start, m_current_unit,
                                    EVENTKIND_KEY, start_pitch);
        if (meas_int.end >= m_pxtn->master->get_meas_num()) {
          m_pxtn->master->set_meas_num(meas_int.end + 1);
          updateGeometry();
        }
        emit onEdit();
        break;
      case MouseEditState::DeleteOn:
        m_pxtn->evels->Record_Delete(clock_int.start, clock_int.end,
                                     m_current_unit, EVENTKIND_ON);
        m_pxtn->evels->Record_Delete(clock_int.start, clock_int.end,
                                     m_current_unit, EVENTKIND_VELOCITY);
        emit onEdit();
        break;
      case MouseEditState::SetNote:
        m_pxtn->evels->Record_Delete(clock_int.start, clock_int.end,
                                     m_current_unit, EVENTKIND_KEY);
        m_pxtn->evels->Record_Add_i(clock_int.start, m_current_unit,
                                    EVENTKIND_KEY, start_pitch);
        if (meas_int.start >= m_pxtn->master->get_meas_num())
          m_pxtn->master->set_meas_num(meas_int.start + 1);
        emit onEdit();
        break;
      case MouseEditState::DeleteNote:
        m_pxtn->evels->Record_Delete(clock_int.start, clock_int.end,
                                     m_current_unit, EVENTKIND_KEY);
        emit onEdit();
        break;
      case MouseEditState::Seek: {
        pxtnVOMITPREPARATION prep{};
        prep.flags |= pxtnVOMITPREPFLAG_loop;
        prep.start_pos_sample = long(m_mouse_edit_state.current_clock) * 60 *
                                44100 / m_pxtn->master->get_beat_clock() /
                                m_pxtn->master->get_beat_tempo();
        prep.master_volume = 0.80f;
        m_pxtn->moo_preparation(&prep);
      } break;
      case MouseEditState::Nothing:
        break;
    }
  m_mouse_edit_state.type =
      (event->modifiers() & Qt::ShiftModifier ? MouseEditState::Type::Seek
                                              : MouseEditState::Type::Nothing);
}
