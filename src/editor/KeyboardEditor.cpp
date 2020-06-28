#include "KeyboardEditor.h"

#include <QDebug>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QTime>

#include "PxtoneUnitIODevice.h"
#include "quantize.h"

int nonnegative_modulo(int x, int m) {
  if (m == 0) return 0;
  return ((x % m) + m) % m;
}
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
// int64_t(m_edit_state.mouse_edit_state.current_clock)
void seekMoo(pxtnService *pxtn, int64_t clock) {
  pxtnVOMITPREPARATION prep{};
  prep.flags |= pxtnVOMITPREPFLAG_loop;
  prep.start_pos_sample = clock * 60 * 44100 / pxtn->master->get_beat_clock() /
                          pxtn->master->get_beat_tempo();
  prep.master_volume = 0.80f;
  pxtn->moo_preparation(&prep);
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
      quantXIndex(0),
      quantizeSelectionY(0),
      m_test_activity(false),
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
  connect(m_client, &Client::receivedAction, this,
          &KeyboardEditor::processRemoteAction);

  connect(this, &KeyboardEditor::editStateChanged,
          [this]() { m_client->sendAction(m_edit_state); });
  connect(m_sync, &PxtoneActionSynchronizer::measureNumChanged,
          [this]() { updateGeometry(); });
}

// It sort of sucks that ServerAction isn't const. This has to do with the fact
// that events that involve reading from a pxtnDescriptor can't be made const
// easily and also be serializable from a file in the same type. Perhaps if the
// pxtone API had a 'read descriptor' separate from a 'write descriptor'...
void KeyboardEditor::processRemoteAction(const ServerAction &a) {
  qint64 uid = a.uid;
  std::visit(
      overloaded{
          [this, uid](const ClientAction &s) {
            std::visit(
                overloaded{
                    [this, uid](const EditState &s) {
                      auto it = m_remote_edit_states.find(uid);
                      if (it == m_remote_edit_states.end())
                        qWarning()
                            << "Received edit state for unknown session" << uid;
                      else
                        it->second.state.emplace(s);
                    },
                    [this, uid](const EditAction &s) {
                      m_sync->applyRemoteAction(s, uid);
                    },
                    [this, uid](const UndoRedo &s) {
                      m_sync->applyUndoRedo(s, uid);
                    },
                    [this, uid](const AddWoice &s) {
                      bool success = m_sync->applyAddWoice(s, uid);
                      emit woicesChanged();
                      if (!success) {
                        qWarning() << "Could not add voice" << s.name
                                   << "from user" << uid;
                        if (uid == m_sync->uid())
                          QMessageBox::warning(
                              this, tr("Could not add voice"),
                              tr("Could not add voice %1").arg(s.name));
                      }
                    },
                    [this, uid](const RemoveWoice &s) {
                      bool success = m_sync->applyRemoveWoice(s, uid);
                      emit woicesChanged();
                      if (!success && uid == m_sync->uid())
                        QMessageBox::warning(
                            this, tr("Could not remove voice"),
                            tr("Could not add remove %1").arg(s.name));
                      else
                        // TODO: Not a great way to handle deleted
                        // woices. But I'm not really sure how else. Probably
                        // related to that task for separating out moo.
                        seekMoo(m_pxtn, m_pxtn->moo_get_now_clock());
                    },
                    [this, uid](const AddUnit &s) {
                      bool success = m_sync->applyAddUnit(s, uid);
                      emit unitsChanged();
                      if (uid == m_sync->uid() && success) {
                        m_edit_state.m_current_unit_id =
                            m_sync->unitIdMap().noToId(m_pxtn->Unit_Num() - 1);
                        emit currentUnitNoChanged(m_pxtn->Unit_Num() - 1);
                      }
                    },
                    [this, uid](const RemoveUnit &s) {
                      auto current_unit_no = m_sync->unitIdMap().idToNo(
                          m_edit_state.m_current_unit_id);
                      m_sync->applyRemoveUnit(s, uid);
                      if (current_unit_no != std::nullopt &&
                          m_pxtn->Unit_Num() <= current_unit_no.value() &&
                          m_pxtn->Unit_Num() > 0) {
                        m_edit_state.m_current_unit_id =
                            m_sync->unitIdMap().noToId(current_unit_no.value() -
                                                       1);
                        emit currentUnitNoChanged(current_unit_no.value() - 1);
                      }

                      emit unitsChanged();
                    }},

                s);
          },
          [this, uid](const NewSession &s) {
            if (m_remote_edit_states.find(uid) != m_remote_edit_states.end())
              qWarning() << "Remote session already exists for uid" << uid
                         << "; overwriting";
            m_remote_edit_states[uid] =
                RemoteEditState{std::nullopt, s.username};
            emit userListChanged(getUserList(m_remote_edit_states));
          },
          [this, uid](const DeleteSession &s) {
            (void)s;
            if (!m_remote_edit_states.erase(uid))
              qWarning() << "Received delete for unknown remote session";
            emit userListChanged(getUserList(m_remote_edit_states));
          },
      },
      a.action);
}

void KeyboardEditor::toggleTestActivity() {
  m_test_activity = !m_test_activity;
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
  Brush(double hue) : Brush(int(hue * 360)){};

  QColor toQColor(int velocity, bool on, int alpha) const {
    int brightness =
        lerp(double(velocity) / EVENTMAX_VELOCITY, muted_brightness,
             on ? on_brightness : base_brightness);
    int saturation = lerp(double(velocity) / EVENTMAX_VELOCITY,
                          muted_saturation, base_saturation);
    return QColor::fromHsl(hue, saturation, brightness, alpha);
  }
};

static Brush brushes[] = {
    0.0 / 7, 3.0 / 7, 6.0 / 7, 2.0 / 7, 5.0 / 7, 1.0 / 7, 4.0 / 7,
};
constexpr int NUM_BRUSHES = sizeof(brushes) / sizeof(Brush);

int pixelsPerVelocity = 3;
static double slack = 50;
int impliedVelocity(MouseEditState state, const Scale &scale) {
  double delta = (state.current_pitch - state.start_pitch) / scale.pitchPerPx;
  // Apply a sigmoid so that small changes in y hardly do anything
  delta = 2 * slack / (1 + exp(2 * delta / slack)) + delta - slack;

  return clamp(round(state.base_velocity + delta / pixelsPerVelocity), 0,
               EVENTMAX_VELOCITY);
}
static QBrush halfWhite(QColor::fromRgb(255, 255, 255, 128));
static qint32 arbitrarily_tall = 512;

void drawVelTooltip(QPainter &painter, qint32 vel, qint32 clock, qint32 pitch,
                    const Brush &brush, const Scale &scale, qint32 alpha) {
  if (alpha == 0) return;
  qint32 draw_vel = (EVENTMAX_VELOCITY + vel) / 2;
  painter.setPen(brush.toQColor(draw_vel, true, alpha));
  painter.setFont(QFont("Sans serif", 6));
  painter.drawText(clock / scale.clockPerPx,
                   scale.pitchToY(pitch) - arbitrarily_tall, arbitrarily_tall,
                   arbitrarily_tall, Qt::AlignBottom, QString("%1").arg(vel));
}
void drawOngoingEdit(const EditState &state, QPainter &painter, int height,
                     double alphaMultiplier) {
  const Brush &brush =
      brushes[nonnegative_modulo(state.m_current_unit_id, NUM_BRUSHES)];
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
                 brush.toQColor(velocity, false, alpha * alphaMultiplier),
                 state.scale);

      paintHighlight(pitch, std::min(interval.start, interval.end), painter,
                     brush.toQColor(128, true, alpha * alphaMultiplier),
                     state.scale);

      if (mouse_edit_state.type == MouseEditState::SetOn)
        drawVelTooltip(painter, velocity, interval.start, pitch, brush,
                       state.scale, 255 * alphaMultiplier);

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

double smoothDistance(double dy, double dx) {
  double r = dy * dy + dx * dx;
  return std::max(0.0, 1 / (r + 1));
}
void drawStateSegment(QPainter &painter, const DrawState &state,
                      const Interval &segment, const Interval &bounds,
                      const Brush &brush, qint32 alpha, const Scale &scale,
                      qint32 current_clock, const MouseEditState &mouse,
                      bool drawTooltip) {
  Interval on = state.ongoingOnEvent.value();
  Interval interval = interval_intersect(on, segment);
  if (interval_intersect(interval, bounds).empty()) return;
  QColor color =
      brush.toQColor(state.velocity.value, on.contains(current_clock), alpha);
  paintBlock(state.pitch.value, interval, painter, color, scale);
  if (interval.start == on.start) {
    paintHighlight(state.pitch.value, interval.start, painter,
                   brush.toQColor(255, true, alpha), scale);
    if (drawTooltip) {
      double alphaMultiplier =
          smoothDistance(
              (mouse.current_clock - on.start) / 40.0 / scale.clockPerPx,
              (mouse.current_pitch - state.pitch.value) / 200.0 /
                  scale.pitchPerPx) *
          0.4;
      if (mouse.current_clock < on.end && mouse.current_clock >= on.start &&
          mouse.type != MouseEditState::SetOn)
        alphaMultiplier += 0.6;
      drawVelTooltip(painter, state.velocity.value, interval.start,
                     state.pitch.value, brush, scale, alpha * alphaMultiplier);
    }
  }
}
// TODO: Make an FPS tracker singleton
static qreal iFps;
constexpr int WINDOW_BOUND_SLACK = 32;
void KeyboardEditor::paintEvent(QPaintEvent *event) {
  ++painted;
  // if (painted > 10) return;
  QPainter painter(this);
  Interval clockBounds = {
      qint32(event->rect().left() * m_edit_state.scale.clockPerPx) -
          WINDOW_BOUND_SLACK,
      qint32(event->rect().right() * m_edit_state.scale.clockPerPx) +
          WINDOW_BOUND_SLACK};

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
  for (int i = 0; i < m_pxtn->Unit_Num(); ++i) drawStates.emplace_back();

  painter.setPen(Qt::blue);
  // TODO: usecs is choppy - it's an upper bound that gets worse with buffer
  // size incrase. for longer songs though and lower end comps we probably do
  // want a bigger buffer. The formula fixes the upper bound issue, but
  // perhaps we can do some smoothing with a linear thing too. int
  // bytes_per_second = 4 * 44100;  // bytes in sample * bytes per second

  // int64_t usecs = m_audio_output->processedUSecs();
  // usecs -= int64_t(m_audio_output->bufferSize()) * 1E6 / bytes_per_second;
  // int clock = usecs * m_pxtn->master->get_beat_tempo() *
  //          m_pxtn->master->get_beat_clock() / 60 / 1000000;

  int clock = m_pxtn->moo_get_now_clock();
  /*int64_t offset_from_buffer =
      m_audio_output->state() == QAudio::ActiveState
          ? m_audio_output->bufferSize()  // - m_audio_output->bytesFree()
          : 0;
  clock -= offset_from_buffer * m_pxtn->master->get_beat_tempo() *
           m_pxtn->master->get_beat_clock() / 60 / bytes_per_second;*/

  int repeat_clock = m_pxtn->master->get_repeat_meas() *
                     m_pxtn->master->get_beat_num() *
                     m_pxtn->master->get_beat_clock();
  int last_clock = m_pxtn->master->get_beat_clock() *
                   m_pxtn->master->get_play_meas() *
                   m_pxtn->master->get_beat_num();
  clock = (clock - repeat_clock) % last_clock + repeat_clock;

  // Draw the note blocks! Upon hitting an event, see if we are able to draw a
  // previous block.
  // TODO: Draw the current unit at the top.
  for (const EVERECORD *e = m_pxtn->evels->get_Records(); e != nullptr;
       e = e->next) {
    if (e->clock > clockBounds.end) break;
    int unit_no = e->unit_no;
    qint32 unit_id = m_sync->unitIdMap().noToId(unit_no);
    DrawState &state = drawStates[unit_no];
    const Brush &brush = brushes[unit_id % NUM_BRUSHES];
    bool matchingUnit = (unit_id == m_edit_state.m_current_unit_id);
    int alpha = (matchingUnit || m_show_all_units ? 255 : 64);
    switch (e->kind) {
      case EVENTKIND_ON:
        // Draw the last block of the previous on event if there's one to
        // draw.
        if (state.ongoingOnEvent.has_value())
          drawStateSegment(painter, state, {state.pitch.clock, e->clock},
                           clockBounds, brush, alpha, m_edit_state.scale, clock,
                           m_edit_state.mouse_edit_state, matchingUnit);

        state.ongoingOnEvent.emplace(Interval{e->clock, e->value + e->clock});
        break;
      case EVENTKIND_VELOCITY:
        state.velocity.set(e);
        break;
      case EVENTKIND_KEY:
        // Maybe draw the previous segment of the current on event.
        if (state.ongoingOnEvent.has_value()) {
          drawStateSegment(painter, state, {state.pitch.clock, e->clock},
                           clockBounds, brush, alpha, m_edit_state.scale, clock,
                           m_edit_state.mouse_edit_state, matchingUnit);
          if (e->clock > state.ongoingOnEvent.value().end)
            state.ongoingOnEvent.reset();
        }
        state.pitch.set(e);
        break;
      default:
        break;
    }
  }

  // After all the events there might be some blocks that are pending a draw.
  for (uint unit_no = 0; unit_no < drawStates.size(); ++unit_no) {
    if (drawStates[unit_no].ongoingOnEvent.has_value()) {
      qint32 unit_id = m_sync->unitIdMap().noToId(unit_no);
      DrawState &state = drawStates[unit_no];
      const Brush &brush = brushes[unit_id % NUM_BRUSHES];
      bool matchingUnit = (unit_id == m_edit_state.m_current_unit_id);
      int alpha = (matchingUnit || m_show_all_units ? 255 : 64);

      drawStateSegment(painter, state,
                       {state.pitch.clock, state.ongoingOnEvent.value().end},
                       clockBounds, brush, alpha, m_edit_state.scale, clock,
                       m_edit_state.mouse_edit_state, matchingUnit);

      state.ongoingOnEvent.reset();
    }
  }

  // Draw ongoing edits
  for (const auto &[uid, remote_state] : m_remote_edit_states) {
    if (uid == m_sync->uid()) continue;
    if (remote_state.state.has_value()) {
      const EditState &state = remote_state.state.value();
      double alphaMultiplier =
          (state.m_current_unit_id == m_edit_state.m_current_unit_id ? 0.7
                                                                     : 0.3);
      EditState adjusted_state(state);
      adjusted_state.scale = m_edit_state.scale;
      drawOngoingEdit(adjusted_state, painter, size().height(),
                      alphaMultiplier);
    }
  }
  drawOngoingEdit(m_edit_state, painter, size().height(), 1);

  // Draw cursors
  for (const auto &[uid, remote_state] : m_remote_edit_states) {
    if (uid == m_sync->uid()) continue;
    if (remote_state.state.has_value()) {
      EditState state = remote_state.state.value();
      state.scale = m_edit_state.scale;  // Position according to our scale
      int unit_id = state.m_current_unit_id;
      // Draw cursor
      QColor color = Qt::white;
      if (unit_id != m_edit_state.m_current_unit_id)
        color = brushes[unit_id % NUM_BRUSHES].toQColor(EVENTMAX_VELOCITY,
                                                        false, 128);
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

  // Simulate activity on a client
  if (m_test_activity) {
    double period = 60;
    double amp = 20;
    m_edit_state.mouse_edit_state.current_clock += amp * sin(painted / period);
    m_edit_state.mouse_edit_state.current_pitch += amp * cos(painted / period);
    emit editStateChanged();
  }
}

void KeyboardEditor::wheelEvent(QWheelEvent *event) {
  qreal delta = event->angleDelta().y();
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->modifiers() & Qt::ShiftModifier) {
      // scale X
      m_edit_state.scale.clockPerPx *= pow(2, delta / 240.0);
      if (m_edit_state.scale.clockPerPx < 0.5)
        m_edit_state.scale.clockPerPx = 0.5;
      if (m_edit_state.scale.clockPerPx > 128)
        m_edit_state.scale.clockPerPx = 128;
    } else {
      // scale Y
      m_edit_state.scale.pitchPerPx *= pow(2, delta / 240.0);
      if (m_edit_state.scale.pitchPerPx < 8) m_edit_state.scale.pitchPerPx = 8;
      if (m_edit_state.scale.pitchPerPx > PITCH_PER_KEY / 4)
        m_edit_state.scale.pitchPerPx = PITCH_PER_KEY / 4;
    }

    // TODO: Probably also want edit state change here

    updateGeometry();
    event->accept();
  } else if (event->modifiers() & Qt::AltModifier) {
    // In this case, alt flips the scroll direction.
    // Maybe alt shift could handle quantize y?
    delta = event->angleDelta().x();
    int size = sizeof(quantizeXOptions) / sizeof(quantizeXOptions[0]);
    if (delta < 0 && quantXIndex < size - 1) quantXIndex++;
    if (delta > 0 && quantXIndex > 0) quantXIndex--;
    refreshQuantSettings();
    emit quantXIndexChanged(quantXIndex);
    event->accept();
  }

  emit editStateChanged();
}

void KeyboardEditor::mousePressEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }
  if (m_pxtn->Unit_Num() == 0) return;
  int clock = event->localPos().x() * m_edit_state.scale.clockPerPx;
  int pitch = m_edit_state.scale.pitchOfY(event->localPos().y());
  int quantized_pitch = quantize(pitch, m_edit_state.m_quantize_pitch) +
                        m_edit_state.m_quantize_pitch;
  int quantized_clock = quantize(clock, m_edit_state.m_quantize_clock);
  NotePreview *note_preview = nullptr;
  MouseEditState::Type type;
  if (event->modifiers() & Qt::ShiftModifier) {
    type = MouseEditState::Type::Seek;
  } else {
    if (event->modifiers() & Qt::ControlModifier) {
      if (event->button() == Qt::RightButton)
        type = MouseEditState::Type::DeleteNote;
      else {
        type = MouseEditState::Type::SetNote;

        auto maybe_unit_no =
            m_sync->unitIdMap().idToNo(m_edit_state.m_current_unit_id);
        if (maybe_unit_no != std::nullopt) {
          qint32 vel = m_edit_state.mouse_edit_state.base_velocity;
          note_preview =
              new NotePreview(m_pxtn, maybe_unit_no.value(), quantized_pitch,
                              quantized_clock, vel, this);
        }
      }
    } else {
      if (event->button() == Qt::RightButton)
        type = MouseEditState::Type::DeleteOn;
      else {
        type = MouseEditState::Type::SetOn;
        auto maybe_unit_no =
            m_sync->unitIdMap().idToNo(m_edit_state.m_current_unit_id);
        if (maybe_unit_no != std::nullopt) {
          qint32 vel = m_edit_state.mouse_edit_state.base_velocity;
          note_preview =
              new NotePreview(m_pxtn, maybe_unit_no.value(), quantized_pitch,
                              quantized_clock, vel, this);
        }
      }
    }
  }
  // TODO: This note preview thing is a bit jank in case unit changes. But
  // it's fairly safe I think because the audio output itself cuts off if the
  // unit disappears.
  if (note_preview != nullptr) m_audio_note_preview = note_preview;
  m_edit_state.mouse_edit_state = MouseEditState{
      type, m_edit_state.mouse_edit_state.base_velocity, clock, pitch, clock,
      pitch};
  emit editStateChanged();
}

void KeyboardEditor::mouseMoveEvent(QMouseEvent *event) {
  MouseEditState &state = m_edit_state.mouse_edit_state;
  if (state.type == MouseEditState::Type::Nothing &&
      event->modifiers() & Qt::ShiftModifier) {
    state.type = MouseEditState::Type::Seek;
  }
  if (state.type == MouseEditState::Type::Seek &&
      !(event->modifiers() & Qt::ShiftModifier)) {
    state.type = MouseEditState::Type::Nothing;
  }
  // TODO: The edit state should just work in pixel coords. Enough
  // places don't really care about the coord in pitch / clock
  // m_edit_state.scale. Update: Okay maybe not because quantize is useful.
  // Maybe we need more heavyweight abstr

  state.current_clock = event->localPos().x() * m_edit_state.scale.clockPerPx;
  state.current_pitch = m_edit_state.scale.pitchOfY(event->localPos().y());
  if (state.type == MouseEditState::Type::Nothing) {
    state.start_clock = state.current_clock;
    state.start_pitch = state.current_pitch;
  }

  if (m_audio_note_preview != nullptr)
    m_audio_note_preview->setVel(
        impliedVelocity(m_edit_state.mouse_edit_state, m_edit_state.scale));
  emit editStateChanged();
  event->ignore();
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
      auto maybe_unit_no =
          m_sync->unitIdMap().idToNo(m_edit_state.m_current_unit_id);
      if (maybe_unit_no == std::nullopt) break;
      qint32 unit_no = nonnegative_modulo(maybe_unit_no.value() + offset,
                                          m_pxtn->Unit_Num());
      // qDebug() << "Changing unit id" << m_edit_state.m_current_unit_id
      //         << m_sync->unitIdMap().noToId(unit_no);
      m_edit_state.m_current_unit_id = m_sync->unitIdMap().noToId(unit_no);
      emit currentUnitNoChanged(unit_no);
      emit editStateChanged();
      break;
  }
}

void KeyboardEditor::setCurrentUnitNo(int unit_no) {
  // qDebug() << "Changing unit id" << m_edit_state.m_current_unit_id
  //         << m_sync->unitIdMap().noToId(unit_no);
  m_edit_state.m_current_unit_id = m_sync->unitIdMap().noToId(unit_no);

  emit editStateChanged();
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

void KeyboardEditor::loadHistory(QList<ServerAction> &history) {
  for (ServerAction &a : history) processRemoteAction(a);
}

void KeyboardEditor::setUid(qint64 uid) { m_sync->setUid(uid); }

void KeyboardEditor::resetUnitIdMap() { m_sync->resetUnitIdMap(); }

void KeyboardEditor::refreshQuantSettings() {
  m_edit_state.m_quantize_clock =
      m_pxtn->master->get_beat_clock() / quantizeXOptions[quantXIndex].second;
  m_edit_state.m_quantize_pitch =
      PITCH_PER_KEY / quantizeYOptions[quantizeSelectionY].second;
}

void KeyboardEditor::setQuantXIndex(int q) {
  quantXIndex = q;
  refreshQuantSettings();
  emit editStateChanged();
}
void KeyboardEditor::setQuantYIndex(int q) {
  quantizeSelectionY = q;
  refreshQuantSettings();
  emit editStateChanged();
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
    std::list<Action> actions;
    switch (m_edit_state.mouse_edit_state.type) {
      case MouseEditState::SetOn:
        actions.push_back({Action::DELETE, EVENTKIND_ON,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::DELETE, EVENTKIND_VELOCITY,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::DELETE, EVENTKIND_KEY,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::ADD, EVENTKIND_ON,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           clock_int.length()});
        m_edit_state.mouse_edit_state.base_velocity =
            impliedVelocity(m_edit_state.mouse_edit_state, m_edit_state.scale);
        actions.push_back({Action::ADD, EVENTKIND_VELOCITY,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           m_edit_state.mouse_edit_state.base_velocity});
        actions.push_back({Action::ADD, EVENTKIND_KEY,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           start_pitch});
        break;
      case MouseEditState::DeleteOn:
        actions.push_back({Action::DELETE, EVENTKIND_ON,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::DELETE, EVENTKIND_VELOCITY,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::DELETE, EVENTKIND_KEY,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           clock_int.end});
        break;
      case MouseEditState::SetNote:
        actions.push_back({Action::DELETE, EVENTKIND_KEY,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           clock_int.end});
        actions.push_back({Action::ADD, EVENTKIND_KEY,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           start_pitch});
        break;
      case MouseEditState::DeleteNote:
        actions.push_back({Action::DELETE, EVENTKIND_KEY,
                           m_edit_state.m_current_unit_id, clock_int.start,
                           clock_int.end});
        break;
      case MouseEditState::Seek: {
        seekMoo(m_pxtn, m_edit_state.mouse_edit_state.current_clock);
      } break;
      case MouseEditState::Nothing:
        break;
    }
    if (actions.size() > 0) {
      m_client->sendAction(m_sync->applyLocalAction(actions));
      // TODO: Change this to like, when the synchronizer receives an
      // action.
      emit onEdit();
    }
    m_edit_state.mouse_edit_state.type = (event->modifiers() & Qt::ShiftModifier
                                              ? MouseEditState::Type::Seek
                                              : MouseEditState::Type::Nothing);
  }
  emit editStateChanged();
  // TODO: maybe would be good to set it up so that edit state change is
  // bundled with the actual remote action
}

void KeyboardEditor::removeCurrentUnit() {
  if (m_pxtn->Unit_Num() > 0)
    m_client->sendAction(RemoveUnit{m_edit_state.m_current_unit_id});
}
