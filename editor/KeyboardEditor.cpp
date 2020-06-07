#include "KeyboardEditor.h"

#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollArea>
#include <QTime>

#include "PxtoneUnitIODevice.h"

int one_over_last_clock(pxtnService const *pxtn) {
  return pxtn->master->get_beat_clock() * (pxtn->master->get_meas_num() + 1) *
         pxtn->master->get_beat_num();
}

QSize KeyboardEditor::sizeHint() const {
  return QSize(one_over_last_clock(m_pxtn) / m_edit_state.scale.clockPerPx,
               m_edit_state.scale.pitchToY(EVENTMIN_KEY));
}
QList<std::pair<qint64, QString>> getUserList(
    const std::unordered_map<qint64, RemoteEditState> &users) {
  QList<std::pair<qint64, QString>> list;
  for (auto it = users.begin(); it != users.end(); ++it)
    list.append(std::make_pair(it->first, it->second.user));
  return list;
}
KeyboardEditor::KeyboardEditor(pxtnService *pxtn, QAudioOutput *audio_output,
                               Client *client, QScrollArea *parent)
    : QWidget(parent),
      m_pxtn(pxtn),
      m_timer(new QElapsedTimer),
      m_show_all_units(false),
      painted(0),
      m_audio_output(audio_output),
      m_audio_note_preview(nullptr),
      m_anim(new Animation(this)),
      // TODO: we probably don't want to have the editor own the client haha.
      // Need some loose coupling thing.
      m_client(client),
      m_sync(new PxtoneActionSynchronizer(0, m_pxtn, this)),
      m_remote_edit_states() {
  m_edit_state.m_quantize_clock = pxtn->master->get_beat_clock();
  m_edit_state.m_quantize_pitch = PITCH_PER_KEY;
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
  connect(m_client, &Client::receivedRemoteAction, m_sync,
          &PxtoneActionSynchronizer::applyRemoteAction);
  connect(m_client, &Client::receivedNewSession,
          [this](const QString &user, qint64 uid) {
            if (m_remote_edit_states.find(uid) != m_remote_edit_states.end())
              qWarning() << "Remote session already exists for uid" << uid
                         << "; overwriting";
            m_remote_edit_states[uid] = RemoteEditState{std::nullopt, user};
            emit userListChanged(getUserList(m_remote_edit_states));
          });
  connect(m_client, &Client::receivedDeleteSession, [this](qint64 uid) {
    if (!m_remote_edit_states.erase(uid))
      qWarning() << "Received delete for unknown remote session";
    emit userListChanged(getUserList(m_remote_edit_states));
  });

  connect(m_client, &Client::receivedEditState,
          [this](const EditStateWithUid &m) {
            auto it = m_remote_edit_states.find(m.uid);
            if (it == m_remote_edit_states.end())
              qWarning() << "Received edit state for unknown session" << m.uid;
            else
              it->second.state.emplace(m.state);
          });

  connect(this, &KeyboardEditor::editStateChanged, m_client,
          &Client::sendEditState);
  connect(m_sync, &PxtoneActionSynchronizer::measureNumChanged,
          [this]() { updateGeometry(); });
}

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

  QColor toQColor(int velocity, bool on, int alpha) const {
    int brightness =
        lerp(double(velocity) / EVENTMAX_VELOCITY, muted_brightness,
             on ? on_brightness : base_brightness);
    int saturation = lerp(double(velocity) / EVENTMAX_VELOCITY,
                          muted_saturation, base_saturation);
    return QColor::fromHsl(hue, saturation, brightness, alpha);
  }
};

int pixelsPerVelocity = 3;
int impliedVelocity(MouseEditState state, const Scale &scale) {
  return clamp(
      EVENTDEFAULT_VELOCITY + (state.current_pitch - state.start_pitch) /
                                  scale.pitchPerPx / pixelsPerVelocity,
      0, EVENTMAX_VELOCITY);
}
static QBrush halfWhite(QColor::fromRgb(255, 255, 255, 128));
void drawOngoingEdit(const EditState &state, QPainter &painter,
                     const std::vector<Brush> &brushes, QPen *pen, int height,
                     double alphaMultiplier) {
  if (uint(state.m_current_unit) >= brushes.size())
    return;  // In case nothing's loaded
  const MouseEditState &mouse_edit_state = state.mouse_edit_state;
  switch (mouse_edit_state.type) {
    case MouseEditState::Type::Nothing:
    case MouseEditState::Type::SetOn:
    case MouseEditState::Type::DeleteOn:
    case MouseEditState::Type::SetNote:
    case MouseEditState::Type::DeleteNote: {
      int velocity = impliedVelocity(mouse_edit_state, state.scale);
      // TODO: maybe factor out this quantization logic
      Interval interval(mouse_edit_state.clock_int(state.m_quantize_clock));
      int pitch =
          quantize(mouse_edit_state.start_pitch, state.m_quantize_pitch) +
          state.m_quantize_pitch;
      int alpha =
          (mouse_edit_state.type == MouseEditState::Nothing ? 128 : 255);
      paintBlock(pitch, interval, painter,
                 brushes[state.m_current_unit].toQColor(
                     velocity, false, alpha * alphaMultiplier),
                 state.scale);

      paintHighlight(pitch, std::min(interval.start, interval.end), painter,
                     brushes[state.m_current_unit].toQColor(
                         255, true, alpha * alphaMultiplier),
                     state.scale);

      if (mouse_edit_state.type != MouseEditState::Nothing && pen != nullptr) {
        painter.setPen(*pen);
        painter.drawText(interval.start / state.scale.clockPerPx,
                         state.scale.pitchToY(pitch),
                         QString("(%1, %2, %3, %4)")
                             .arg(interval.start)
                             .arg(interval.end)
                             .arg(pitch)
                             .arg(velocity));
      }
    } break;
    case MouseEditState::Type::Seek:
      painter.fillRect(mouse_edit_state.current_clock / state.scale.clockPerPx,
                       0, 1, height,
                       QColor::fromRgb(255, 255, 255, 128 * alphaMultiplier));
      break;
  }
}

void drawCursor(const EditState &state, QPainter &painter, const QColor &color,
                const QString &username, qint64 uid) {
  QPoint position(state.mouse_edit_state.current_clock / state.scale.clockPerPx,
                  state.scale.pitchToY(state.mouse_edit_state.current_pitch));
  QPainterPath path;
  path.moveTo(position);
  path.lineTo(position + QPoint(8, 0));
  path.lineTo(position + QPoint(0, 8));
  path.closeSubpath();
  painter.fillPath(path, color);
  painter.setPen(color);
  painter.setFont(QFont("Sans serif", 6));
  painter.drawText(position + QPoint(8, 13),
                   QString("%1 (%2)").arg(username).arg(uid));
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
    int x =
        m_pxtn->master->get_beat_clock() * beat / m_edit_state.scale.clockPerPx;
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

    if (row * PITCH_PER_KEY / m_edit_state.scale.pitchPerPx > size().height())
      break;
    int this_y = row * PITCH_PER_KEY / m_edit_state.scale.pitchPerPx;
    // Because of rounding error, calculate height by subbing next from this
    // TODO: change scale so that pitchPerPx is necessarily an int
    int next_y = (row + 1) * PITCH_PER_KEY / m_edit_state.scale.pitchPerPx;
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
  // second */; int64_t usecs = m_audio_output->processedUSecs() -
  // int64_t(m_audio_output->bufferSize()) * 10E5 / bytes_per_second;

  /*int64_t usecs = m_audio_output->processedUSecs();
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
    int alpha =
        (i == m_edit_state.m_current_unit || m_show_all_units ? 255 : 64);
    switch (e->kind) {
      case EVENTKIND_ON:
        // Draw the last block of the previous on event if there's one to draw.
        // TODO: This 'draw previous note block' is duplicated quite a bit.
        if (drawStates[i].ongoingOnEvent.has_value()) {
          Interval on = drawStates[i].ongoingOnEvent.value();
          int start = std::max(drawStates[i].pitch.clock, on.start);
          int end = std::min(e->clock, on.end);
          Interval interval{start, end};
          QBrush brush = brushes[i].toQColor(drawStates[i].velocity.value,
                                             on.contains(clock), alpha);
          paintBlock(drawStates[i].pitch.value, interval, painter, brush,
                     m_edit_state.scale);
          if (start == on.start)
            paintHighlight(drawStates[i].pitch.value, start, painter,
                           brushes[i].toQColor(255, true, alpha),
                           m_edit_state.scale);
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
          QBrush brush = brushes[i].toQColor(drawStates[i].velocity.value,
                                             on.contains(clock), alpha);
          paintBlock(drawStates[i].pitch.value, interval, painter, brush,
                     m_edit_state.scale);
          if (start == on.start)
            paintHighlight(drawStates[i].pitch.value, start, painter,
                           brushes[i].toQColor(255, true, alpha),
                           m_edit_state.scale);
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
      int alpha =
          (int(i) == m_edit_state.m_current_unit || m_show_all_units ? 255
                                                                     : 64);
      Interval on = drawStates[i].ongoingOnEvent.value();
      int start = std::max(drawStates[i].pitch.clock, on.start);
      Interval interval{start, on.end};
      QBrush brush = brushes[i].toQColor(drawStates[i].velocity.value,
                                         on.contains(clock), alpha);
      paintBlock(drawStates[i].pitch.value, interval, painter, brush,
                 m_edit_state.scale);
      if (start == on.start)
        paintBlock(drawStates[i].pitch.value,
                   {start, start + 2 * int(m_edit_state.scale.clockPerPx)},
                   painter, brushes[i].toQColor(255, true, alpha),
                   m_edit_state.scale);
      drawStates[i].ongoingOnEvent.reset();
    }
  }

  // Draw ongoing edits
  for (const auto &[uid, remote_state] : m_remote_edit_states) {
    if (uid == m_sync->uid()) continue;
    if (remote_state.state.has_value()) {
      const EditState &state = remote_state.state.value();
      double alphaMultiplier =
          (state.m_current_unit == m_edit_state.m_current_unit ? 0.7 : 0.3);
      EditState adjusted_state(state);
      adjusted_state.scale = m_edit_state.scale;
      drawOngoingEdit(adjusted_state, painter, brushes, nullptr,
                      size().height(), alphaMultiplier);
    }
  }
  drawOngoingEdit(m_edit_state, painter, brushes, &pen, size().height(), 1);

  // Draw cursors
  for (const auto &[uid, remote_state] : m_remote_edit_states) {
    if (uid == m_sync->uid()) continue;
    if (remote_state.state.has_value()) {
      EditState state = remote_state.state.value();
      state.scale = m_edit_state.scale;  // Position according to our scale
      int unit = state.m_current_unit;
      if (uint(unit) >= brushes.size()) continue;
      // Draw cursor
      QColor color = Qt::white;
      if (unit != m_edit_state.m_current_unit)
        color = brushes[unit].toQColor(EVENTMAX_VELOCITY, false, 128);
      drawCursor(state, painter, color, remote_state.user, uid);
    }
  }
  {
    QString my_username = "";
    auto it = m_remote_edit_states.find(m_sync->uid());
    if (it != m_remote_edit_states.end()) my_username = it->second.user;
    drawCursor(m_edit_state, painter, Qt::white, my_username, m_sync->uid());
  }

  // clock = us * 1s/10^6us * 1m/60s * tempo beats/m * beat_clock clock/beats
  // Draw the current player position
  painter.fillRect(clock / m_edit_state.scale.clockPerPx, 0, 1, size().height(),
                   Qt::white);

  // Draw bars at the end of the piece
  painter.fillRect(last_clock / m_edit_state.scale.clockPerPx, 0, 1,
                   size().height(), halfWhite);
  painter.fillRect(m_pxtn->moo_get_end_clock() / m_edit_state.scale.clockPerPx,
                   0, 1, size().height(), halfWhite);
}

void KeyboardEditor::wheelEvent(QWheelEvent *event) {
  QPoint delta = event->angleDelta();
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->modifiers() & Qt::ShiftModifier) {
      // scale X
      m_edit_state.scale.clockPerPx *= pow(2, delta.y() / 240.0);
      if (m_edit_state.scale.clockPerPx < 0.5)
        m_edit_state.scale.clockPerPx = 0.5;
      if (m_edit_state.scale.clockPerPx > 128)
        m_edit_state.scale.clockPerPx = 128;
    } else {
      // scale Y
      m_edit_state.scale.pitchPerPx *= pow(2, delta.y() / 240.0);
      if (m_edit_state.scale.pitchPerPx < 8) m_edit_state.scale.pitchPerPx = 8;
      if (m_edit_state.scale.pitchPerPx > PITCH_PER_KEY / 4)
        m_edit_state.scale.pitchPerPx = PITCH_PER_KEY / 4;
    }

    // TODO: Probably also want edit state change here

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
      new PxtoneUnitIODevice(audio, m_pxtn, m_edit_state.m_current_unit, pitch);
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
  int clock = event->localPos().x() * m_edit_state.scale.clockPerPx;
  int pitch = m_edit_state.scale.pitchOfY(event->localPos().y());
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
        audio = make_audio(quantize(pitch, m_edit_state.m_quantize_pitch) +
                           m_edit_state.m_quantize_pitch);
      }
    } else {
      if (event->button() == Qt::RightButton)
        type = MouseEditState::Type::DeleteOn;
      else {
        type = MouseEditState::Type::SetOn;
        audio = make_audio(quantize(pitch, m_edit_state.m_quantize_pitch) +
                           m_edit_state.m_quantize_pitch);
      }
    }
  }
  m_audio_note_preview = audio;
  m_edit_state.mouse_edit_state =
      MouseEditState{type, clock, pitch, clock, pitch};
  emit editStateChanged(m_edit_state);
}

void KeyboardEditor::mouseMoveEvent(QMouseEvent *event) {
  if (m_edit_state.mouse_edit_state.type == MouseEditState::Type::Nothing &&
      event->modifiers() & Qt::ShiftModifier) {
    m_edit_state.mouse_edit_state.type = MouseEditState::Type::Seek;
  }
  if (m_edit_state.mouse_edit_state.type == MouseEditState::Type::Seek &&
      !(event->modifiers() & Qt::ShiftModifier)) {
    m_edit_state.mouse_edit_state.type = MouseEditState::Type::Nothing;
  }
  // TODO: The edit state should just work in pixel coords. Enough
  // places don't really care about the coord in pitch / clock
  // m_edit_state.scale. Update: Okay maybe not because quantize is useful.
  // Maybe we need more heavyweight abstr

  m_edit_state.mouse_edit_state.current_clock =
      event->localPos().x() * m_edit_state.scale.clockPerPx;
  m_edit_state.mouse_edit_state.current_pitch =
      m_edit_state.scale.pitchOfY(event->localPos().y());
  if (m_edit_state.mouse_edit_state.type == MouseEditState::Type::Nothing) {
    m_edit_state.mouse_edit_state.start_clock =
        m_edit_state.mouse_edit_state.current_clock;
    m_edit_state.mouse_edit_state.start_pitch =
        m_edit_state.mouse_edit_state.current_pitch;
  }
  emit editStateChanged(m_edit_state);
  event->ignore();
}

int nonnegative_modulo(int x, int m) {
  if (m == 0) return 0;
  return ((x % m) + m) % m;
}
void KeyboardEditor::cycleCurrentUnit(int offset) {
  switch (m_edit_state.mouse_edit_state.type) {
    case MouseEditState::Type::SetOn:
    case MouseEditState::Type::DeleteOn:
    case MouseEditState::Type::SetNote:
    case MouseEditState::Type::DeleteNote:
      break;
    case MouseEditState::Type::Nothing:
    case MouseEditState::Type::Seek:
      m_edit_state.m_current_unit = nonnegative_modulo(
          m_edit_state.m_current_unit + offset, m_pxtn->Unit_Num());
      emit currentUnitChanged(m_edit_state.m_current_unit);
      break;
  }
  emit editStateChanged(m_edit_state);
}

void KeyboardEditor::setCurrentUnit(int unit) {
  m_edit_state.m_current_unit = unit;
  emit editStateChanged(m_edit_state);
}

void KeyboardEditor::setShowAll(bool b) { m_show_all_units = b; }

void KeyboardEditor::clearRemoteEditStates() {
  m_remote_edit_states.clear();

  emit userListChanged(getUserList(m_remote_edit_states));
}

void KeyboardEditor::toggleShowAllUnits() {
  m_show_all_units = !m_show_all_units;
  emit showAllChanged(m_show_all_units);
}

void KeyboardEditor::loadHistory(const QList<RemoteActionWithUid> &history) {
  for (const RemoteActionWithUid &action : history) {
    m_sync->applyRemoteAction(action);
  }
}

void KeyboardEditor::setUid(qint64 uid) { m_sync->setUid(uid); }

void KeyboardEditor::setQuantX(int q) {
  m_edit_state.m_quantize_clock = m_pxtn->master->get_beat_clock() / q;
  emit editStateChanged(m_edit_state);
}
void KeyboardEditor::setQuantY(int q) {
  m_edit_state.m_quantize_pitch = PITCH_PER_KEY / q;
  emit editStateChanged(m_edit_state);
}

void KeyboardEditor::mouseReleaseEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }
  if (m_audio_note_preview) {
    m_audio_note_preview->suspend();
    m_audio_note_preview->deleteLater();
    m_audio_note_preview = nullptr;
  }
  Interval clock_int(
      m_edit_state.mouse_edit_state.clock_int(m_edit_state.m_quantize_clock));
  int start_pitch = quantize(m_edit_state.mouse_edit_state.start_pitch,
                             m_edit_state.m_quantize_pitch) +
                    m_edit_state.m_quantize_pitch;
  // int end_pitch = int(round(pitchOfY(event->localPos().y())));

  if (m_pxtn->Unit_Num() > 0) {
    std::vector<Action> actions;
    switch (m_edit_state.mouse_edit_state.type) {
      case MouseEditState::SetOn:
        actions.push_back({Action::DELETE, EVENTKIND_ON,
                           m_edit_state.m_current_unit, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::DELETE, EVENTKIND_VELOCITY,
                           m_edit_state.m_current_unit, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::DELETE, EVENTKIND_KEY,
                           m_edit_state.m_current_unit, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::ADD, EVENTKIND_ON,
                           m_edit_state.m_current_unit, clock_int.start,
                           clock_int.length()});
        actions.push_back({Action::ADD, EVENTKIND_VELOCITY,
                           m_edit_state.m_current_unit, clock_int.start,
                           impliedVelocity(m_edit_state.mouse_edit_state,
                                           m_edit_state.scale)});
        actions.push_back({Action::ADD, EVENTKIND_KEY,
                           m_edit_state.m_current_unit, clock_int.start,
                           start_pitch});

        break;
      case MouseEditState::DeleteOn:
        actions.push_back({Action::DELETE, EVENTKIND_ON,
                           m_edit_state.m_current_unit, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::DELETE, EVENTKIND_VELOCITY,
                           m_edit_state.m_current_unit, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::DELETE, EVENTKIND_KEY,
                           m_edit_state.m_current_unit, clock_int.start,
                           clock_int.end});
        break;
      case MouseEditState::SetNote:
        actions.push_back({Action::DELETE, EVENTKIND_KEY,
                           m_edit_state.m_current_unit, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::ADD, EVENTKIND_KEY,
                           m_edit_state.m_current_unit, clock_int.start,
                           start_pitch});
        break;
      case MouseEditState::DeleteNote:
        actions.push_back({Action::DELETE, EVENTKIND_KEY,
                           m_edit_state.m_current_unit, clock_int.start,
                           clock_int.end});
        break;
      case MouseEditState::Seek: {
        pxtnVOMITPREPARATION prep{};
        prep.flags |= pxtnVOMITPREPFLAG_loop;
        prep.start_pos_sample =
            int64_t(m_edit_state.mouse_edit_state.current_clock) * 60 * 44100 /
            m_pxtn->master->get_beat_clock() / m_pxtn->master->get_beat_tempo();
        prep.master_volume = 0.80f;
        m_pxtn->moo_preparation(&prep);
      } break;
      case MouseEditState::Nothing:
        break;
    }
    if (actions.size() > 0) {
      m_client->sendRemoteAction(m_sync->applyLocalAction(actions));
      // TODO: Change this to like, when the synchronizer receives an action.
      emit onEdit();
    }
    m_edit_state.mouse_edit_state.type = (event->modifiers() & Qt::ShiftModifier
                                              ? MouseEditState::Type::Seek
                                              : MouseEditState::Type::Nothing);
  }
  emit editStateChanged(m_edit_state);
  // TODO: maybe would be good to set it up so that edit state change is
  // bundled with the actual remote action
}

void KeyboardEditor::undo() { m_client->sendRemoteAction(m_sync->getUndo()); }

void KeyboardEditor::redo() { m_client->sendRemoteAction(m_sync->getRedo()); }
