#include "KeyboardView.h"

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

QSize KeyboardView::sizeHint() const {
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
void seekMoo(const pxtnService *pxtn, mooState *moo_state, int64_t clock) {
  pxtnVOMITPREPARATION prep{};
  prep.flags |= pxtnVOMITPREPFLAG_loop | pxtnVOMITPREPFLAG_unit_mute;
  prep.start_pos_sample = clock * 60 * 44100 / pxtn->master->get_beat_clock() /
                          pxtn->master->get_beat_tempo();
  prep.master_volume = 0.80f;
  bool success = pxtn->moo_preparation(&prep, *moo_state);
  if (!success) qWarning() << "Moo preparation error";
}

KeyboardView::KeyboardView(const pxtnService *pxtn, mooState *moo_state,
                           QAudioOutput *audio_output,
                           PxtoneController *controller, Client *client,
                           UnitListModel *units, QScrollArea *parent)
    : QWidget(parent),
      m_pxtn(pxtn),
      m_moo_state(moo_state),
      m_timer(new QElapsedTimer),
      painted(0),
      m_audio_output(audio_output),
      m_audio_note_preview(nullptr),
      m_anim(new Animation(this)),
      // TODO: we probably don't want to have the editor own the client haha.
      // Need some loose coupling thing.
      m_client(client),
      m_controller(controller),
      quantXIndex(0),
      quantizeSelectionY(0),
      m_last_clock(0),
      m_this_seek(0),
      m_this_seek_caught_up(false),
      m_test_activity(false),
      m_remote_edit_states(),
      m_clipboard(m_pxtn),
      m_units(units) {
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
          &KeyboardView::processRemoteAction);

  connect(this, &KeyboardView::editStateChanged,
          [this]() { m_client->sendAction(m_edit_state); });
  connect(m_controller, &PxtoneController::measureNumChanged,
          [this]() { updateGeometry(); });
}

// It sort of sucks that ServerAction isn't const. This has to do with the fact
// that events that involve reading from a pxtnDescriptor can't be made const
// easily and also be serializable from a file in the same type. Perhaps if the
// pxtone API had a 'read descriptor' separate from a 'write descriptor'...
void KeyboardView::processRemoteAction(const ServerAction &a) {
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
                      m_controller->applyRemoteAction(s, uid);
                    },
                    [this, uid](const UndoRedo &s) {
                      m_controller->applyUndoRedo(s, uid);
                    },
                    [this, uid](const TempoChange &s) {
                      if (m_controller->applyTempoChange(s, uid))
                        emit tempoBeatChanged();
                    },
                    [this, uid](const BeatChange &s) {
                      if (m_controller->applyBeatChange(s, uid))
                        emit tempoBeatChanged();
                    },
                    [this, uid](const AddWoice &s) {
                      bool success = m_controller->applyAddWoice(s, uid);
                      emit woicesChanged();
                      if (!success) {
                        qWarning() << "Could not add voice" << s.name
                                   << "from user" << uid;
                        if (uid == m_controller->uid())
                          QMessageBox::warning(
                              this, tr("Could not add voice"),
                              tr("Could not add voice %1").arg(s.name));
                      }
                    },
                    [this, uid](const RemoveWoice &s) {
                      bool success = m_controller->applyRemoveWoice(s, uid);
                      emit woicesChanged();
                      if (!success && uid == m_controller->uid())
                        QMessageBox::warning(
                            this, tr("Could not remove voice"),
                            tr("Could not add remove %1").arg(s.name));
                      else
                        // Refresh units using the woice.
                        // (No refresh is also fine but just stale)
                        seekMoo(m_pxtn, m_moo_state,
                                m_pxtn->moo_get_now_clock(*m_moo_state));
                    },
                    [this, uid](const ChangeWoice &s) {
                      bool success = m_controller->applyChangeWoice(s, uid);
                      emit woicesChanged();
                      if (!success && uid == m_controller->uid())
                        QMessageBox::warning(
                            this, tr("Could not change voice"),
                            tr("Could not add change %1").arg(s.remove.name));
                      else
                        seekMoo(m_pxtn, m_moo_state,
                                m_pxtn->moo_get_now_clock(*m_moo_state));
                    },
                    [this, uid](const AddUnit &s) {
                      m_units->beginAdd();
                      bool success = m_controller->applyAddUnit(s, uid);
                      m_units->endAdd();
                      if (uid == m_controller->uid() && success) {
                        m_edit_state.m_current_unit_id =
                            m_controller->unitIdMap().noToId(
                                m_pxtn->Unit_Num() - 1);
                        emit currentUnitNoChanged(m_pxtn->Unit_Num() - 1);
                      }
                    },
                    [this, uid](const RemoveUnit &s) {
                      auto current_unit_no = m_controller->unitIdMap().idToNo(
                          m_edit_state.m_current_unit_id);
                      m_units->beginRemove(current_unit_no.value());
                      m_controller->applyRemoveUnit(s, uid);
                      m_units->endRemove();
                      if (current_unit_no != std::nullopt) {
                        if (m_pxtn->Unit_Num() <= current_unit_no.value() &&
                            m_pxtn->Unit_Num() > 0) {
                          m_edit_state.m_current_unit_id =
                              m_controller->unitIdMap().noToId(
                                  current_unit_no.value() - 1);
                          emit currentUnitNoChanged(current_unit_no.value() -
                                                    1);
                        }
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

void KeyboardView::toggleTestActivity() { m_test_activity = !m_test_activity; }

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

static void drawAtClockPitch(int clock, int pitch, int widthInPx,
                             QPainter &painter, const Scale &scale) {
  int rowHeight = PITCH_PER_KEY / scale.pitchPerPx;
  painter.drawRect(clock / scale.clockPerPx - 1,
                   scale.pitchToY(pitch) + rowHeight / 6 - 1, widthInPx + 1,
                   rowHeight * 2 / 3 + 1);
}

static void paintBlock(int pitch, const Interval &segment, QPainter &painter,
                       const QBrush &brush, const Scale &scale) {
  paintAtClockPitch(segment.start, pitch, segment.length() / scale.clockPerPx,
                    painter, brush, scale);
}

static void drawBlock(int pitch, const Interval &segment, QPainter &painter,
                      const Scale &scale) {
  drawAtClockPitch(segment.start, pitch, segment.length() / scale.clockPerPx,
                   painter, scale);
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
template <typename T>
static T clamp(T x, T lo, T hi) {
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
static QColor halfWhite(QColor::fromRgb(255, 255, 255, 128));
static QColor slightTint(QColor::fromRgb(255, 255, 255, 32));

int pixelsPerVelocity = 3;
static double slack = 50;
int impliedVelocity(MouseEditState state, const Scale &scale) {
  double delta = (state.current_pitch - state.start_pitch) / scale.pitchPerPx;
  // Apply a sigmoid so that small changes in y hardly do anything
  delta = 2 * slack / (1 + exp(2 * delta / slack)) + delta - slack;

  return clamp(int(round(state.base_velocity + delta / pixelsPerVelocity)), 0,
               EVENTMAX_VELOCITY);
}
static qint32 arbitrarily_tall = 512;

void drawSelection(QPainter &painter, const Interval &interval, qint32 height,
                   double alphaMultiplier) {
  QColor c = slightTint;
  c.setAlpha(c.alpha() * alphaMultiplier);
  painter.fillRect(interval.start, 0, interval.length(), height, c);
  c = halfWhite;
  c.setAlpha(c.alpha() * alphaMultiplier);
  painter.fillRect(interval.start, 0, 1, height, c);
  painter.fillRect(interval.end, 0, 1, height, c);
}

void drawExistingSelection(QPainter &painter, const EditState &state,
                           qint32 height, double alphaMultiplier) {
  if (state.mouse_edit_state.selection.has_value()) {
    Interval interval{state.mouse_edit_state.selection.value() /
                      state.scale.clockPerPx};
    drawSelection(painter, interval, height, alphaMultiplier * 0.8);
  }
}

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
void drawOngoingAction(const EditState &state, QPainter &painter, int height,
                       double alphaMultiplier,
                       double selectionAlphaMultiplier) {
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
    case MouseEditState::Type::Select: {
      Interval interval(mouse_edit_state.clock_int(state.m_quantize_clock) /
                        state.scale.clockPerPx);
      drawSelection(painter, interval, height, selectionAlphaMultiplier);
    } break;
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
                      const Interval &segment,
                      const std::optional<Interval> &selection,
                      const Interval &bounds, const Brush &brush, qint32 alpha,
                      const Scale &scale, qint32 current_clock,
                      const MouseEditState &mouse, bool drawTooltip,
                      bool muted) {
  Interval on = state.ongoingOnEvent.value();
  Interval interval = interval_intersect(on, segment);
  if (interval_intersect(interval, bounds).empty()) return;
  QColor color = brush.toQColor(state.velocity.value,
                                on.contains(current_clock) && !muted, alpha);
  if (muted)
    color.setHsl(0, color.saturation() * 0.3, color.lightness(), color.alpha());
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
      else if (selection.has_value() && selection.value().contains(on.start))
        alphaMultiplier += 0.3;
      drawVelTooltip(painter, state.velocity.value, interval.start,
                     state.pitch.value, brush, scale, alpha * alphaMultiplier);
    }
  }
  if (selection.has_value()) {
    Interval selection_segment =
        interval_intersect(selection.value(), interval);
    if (!selection_segment.empty()) {
      painter.setPen(brush.toQColor(EVENTDEFAULT_VELOCITY, true, alpha));
      drawBlock(state.pitch.value, selection_segment, painter, scale);
    }
  }
}
// TODO: Make an FPS tracker singleton
static qreal iFps;
constexpr int WINDOW_BOUND_SLACK = 32;
void KeyboardView::paintEvent(QPaintEvent *event) {
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

  int clock = m_pxtn->moo_get_now_clock(*m_moo_state);
  // Some really hacky magic to get the playhead smoother given that
  // there's a ton of buffering that makes it hard to actually tell where the
  // playhead is accurately. We do this:
  // 1. Subtract the buffer duration.
  // 2. Track how long since the last (laggy) clock update and add.
  // 3. Works most of the time, but leads to a wrong clock after a seek or at
  // the end of a song, because it'll put the playhead before the repeat or
  // the seek position.
  // 4. To account for this, if we're before the current seek position, clamp
  // to seek position. Also track if we've looped and render at the end of
  // song instead of before repeat if so.
  // 5. The tracking if we've looped or not is also necessary to tell if we've
  // actually caught up to a seek.
  int bytes_per_second = 4 * 44100;  // bytes in sample * bytes per second
  if (m_last_clock != clock) {
    m_last_clock = clock;
    timeSinceLastClock.restart();
  }

  int repeat_clock = m_pxtn->master->get_repeat_meas() *
                     m_pxtn->master->get_beat_num() *
                     m_pxtn->master->get_beat_clock();
  int last_clock = m_pxtn->master->get_beat_clock() *
                   m_pxtn->master->get_play_meas() *
                   m_pxtn->master->get_beat_num();

  int64_t offset_from_buffer =
      m_audio_output->bufferSize()  // - m_audio_output->bytesFree()
      ;

  double estimated_buffer_offset =
      -offset_from_buffer / double(bytes_per_second);
  if (m_audio_output->state() != QAudio::ActiveState)
    timeSinceLastClock.restart();
  else
    estimated_buffer_offset += timeSinceLastClock.elapsed() / 1000.0;
  clock += (last_clock - repeat_clock) * m_moo_state->num_loop;
  clock += std::min(estimated_buffer_offset, 0.0) *
           m_pxtn->master->get_beat_tempo() * m_pxtn->master->get_beat_clock() /
           60;
  if (clock >= m_this_seek) m_this_seek_caught_up = true;
  if (!m_this_seek_caught_up) clock = m_this_seek;

  if (clock >= last_clock)
    clock = (clock - repeat_clock) % (last_clock - repeat_clock) + repeat_clock;
  // Because of offsetting it might seem like even though we've repeated the
  // clock is before [repeat_clock]. So fix it here.
  if (m_moo_state->num_loop > 0 && clock < repeat_clock)
    clock += last_clock - repeat_clock;

  // Draw the note blocks! Upon hitting an event, see if we are able to draw a
  // previous block.
  // TODO: Draw the current unit at the top.
  std::optional<Interval> selection = std::nullopt;
  std::set<int> selected_unit_nos = selectedUnitNos();
  if (m_edit_state.mouse_edit_state.selection.has_value())
    selection = m_edit_state.mouse_edit_state.selection.value();
  for (const EVERECORD *e = m_pxtn->evels->get_Records(); e != nullptr;
       e = e->next) {
    if (e->clock > clockBounds.end) break;
    int unit_no = e->unit_no;
    qint32 unit_id = m_controller->unitIdMap().noToId(unit_no);
    DrawState &state = drawStates[unit_no];
    const Brush &brush = brushes[unit_id % NUM_BRUSHES];
    bool matchingUnit = (unit_id == m_edit_state.m_current_unit_id);
    std::optional<Interval> thisSelection = std::nullopt;
    if (selection.has_value() &&
        selected_unit_nos.find(unit_no) != selected_unit_nos.end())
      thisSelection = selection;
    int alpha;
    if (matchingUnit)
      alpha = 255;
    else if (m_pxtn->Unit_Get(unit_no)->get_visible())
      alpha = 64;
    else
      alpha = 0;
    bool muted = !m_pxtn->Unit_Get(unit_no)->get_played();
    switch (e->kind) {
      case EVENTKIND_ON:
        // Draw the last block of the previous on event if there's one to
        // draw.
        if (state.ongoingOnEvent.has_value())
          drawStateSegment(painter, state, {state.pitch.clock, e->clock},
                           thisSelection, clockBounds, brush, alpha,
                           m_edit_state.scale, clock,
                           m_edit_state.mouse_edit_state, matchingUnit, muted);

        state.ongoingOnEvent.emplace(Interval{e->clock, e->value + e->clock});
        break;
      case EVENTKIND_VELOCITY:
        state.velocity.set(e);
        break;
      case EVENTKIND_KEY:
        // Maybe draw the previous segment of the current on event.
        if (state.ongoingOnEvent.has_value()) {
          drawStateSegment(painter, state, {state.pitch.clock, e->clock},
                           thisSelection, clockBounds, brush, alpha,
                           m_edit_state.scale, clock,
                           m_edit_state.mouse_edit_state, matchingUnit, muted);
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
      qint32 unit_id = m_controller->unitIdMap().noToId(unit_no);
      DrawState &state = drawStates[unit_no];
      const Brush &brush = brushes[unit_id % NUM_BRUSHES];
      bool matchingUnit = (unit_id == m_edit_state.m_current_unit_id);
      int alpha;
      if (matchingUnit)
        alpha = 255;
      else if (m_pxtn->Unit_Get(unit_no)->get_visible())
        alpha = 64;
      else
        alpha = 0;
      bool muted = !m_pxtn->Unit_Get(unit_no)->get_played();
      std::optional<Interval> thisSelection = std::nullopt;
      if (selection.has_value() &&
          selected_unit_nos.find(unit_no) != selected_unit_nos.end())
        thisSelection = selection;
      drawStateSegment(
          painter, state, {state.pitch.clock, state.ongoingOnEvent.value().end},
          thisSelection, clockBounds, brush, alpha, m_edit_state.scale, clock,
          m_edit_state.mouse_edit_state, matchingUnit, muted);

      state.ongoingOnEvent.reset();
    }
  }

  // Draw existing selections

  // Draw selections & ongoing edits / selections / seeks
  for (const auto &[uid, remote_state] : m_remote_edit_states) {
    if (uid == m_controller->uid()) continue;
    if (remote_state.state.has_value()) {
      const EditState &state = remote_state.state.value();
      bool same_unit =
          state.m_current_unit_id == m_edit_state.m_current_unit_id;
      double alphaMultiplier = (same_unit ? 0.7 : 0.3);
      double selectionAlphaMultiplier = (same_unit ? 0.5 : 0.3);
      EditState adjusted_state(state);
      adjusted_state.scale = m_edit_state.scale;

      // TODO: colour other's existing selections (or identify somehow. maybe
      // name tag?)
      drawExistingSelection(painter, adjusted_state, size().height(),
                            selectionAlphaMultiplier);
      drawOngoingAction(adjusted_state, painter, size().height(),
                        alphaMultiplier, selectionAlphaMultiplier);
    }
  }
  drawExistingSelection(painter, m_edit_state, size().height(), 1);
  drawOngoingAction(m_edit_state, painter, size().height(), 1, 1);

  // Draw cursors
  for (const auto &[uid, remote_state] : m_remote_edit_states) {
    if (uid == m_controller->uid()) continue;
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
    auto it = m_remote_edit_states.find(m_controller->uid());
    if (it != m_remote_edit_states.end()) my_username = it->second.user;
    drawCursor(m_edit_state, painter, Qt::white, my_username,
               m_controller->uid());
  }

  // clock = us * 1s/10^6us * 1m/60s * tempo beats/m * beat_clock clock/beats
  // Draw the current player position
  painter.fillRect(clock / m_edit_state.scale.clockPerPx, 0, 1, size().height(),
                   (m_this_seek_caught_up ? Qt::white : halfWhite));

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

void KeyboardView::wheelEvent(QWheelEvent *event) {
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
  } else if (m_edit_state.mouse_edit_state.type == MouseEditState::SetOn) {
    auto &vel = m_edit_state.mouse_edit_state.base_velocity;
    vel = clamp<double>(vel + event->angleDelta().y() * 8.0 / 120, 0,
                        EVENTMAX_VELOCITY);
    if (m_audio_note_preview != nullptr)
      m_audio_note_preview->setVel(
          impliedVelocity(m_edit_state.mouse_edit_state, m_edit_state.scale));
    event->accept();
  }

  emit editStateChanged();
}

void updateStatePositions(EditState &edit_state, const QMouseEvent *event) {
  MouseEditState &state = edit_state.mouse_edit_state;
  state.current_clock =
      std::max(0., event->localPos().x() * edit_state.scale.clockPerPx);
  state.current_pitch = edit_state.scale.pitchOfY(event->localPos().y());

  if (state.type == MouseEditState::Type::Nothing ||
      state.type == MouseEditState::Type::Seek) {
    state.type = (event->modifiers() & Qt::ShiftModifier
                      ? MouseEditState::Type::Seek
                      : MouseEditState::Type::Nothing);
    state.start_clock = state.current_clock;
    state.start_pitch = state.current_pitch;
  }
}

void KeyboardView::mousePressEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }

  if (m_pxtn->Unit_Num() == 0) return;
  updateStatePositions(m_edit_state, event);

  bool make_note_preview = false;
  MouseEditState::Type type;
  if (event->modifiers() & Qt::ShiftModifier) {
    if (event->modifiers() & Qt::ControlModifier &&
        event->button() != Qt::RightButton)
      type = MouseEditState::Type::Select;
    else {
      if (event->button() == Qt::RightButton) deselect();
      type = MouseEditState::Type::Seek;
    }
  } else {
    if (event->modifiers() & Qt::ControlModifier) {
      if (event->button() == Qt::RightButton)
        type = MouseEditState::Type::DeleteNote;
      else {
        type = MouseEditState::Type::SetNote;
        make_note_preview = true;
      }
    } else {
      if (event->button() == Qt::RightButton)
        type = MouseEditState::Type::DeleteOn;
      else {
        type = MouseEditState::Type::SetOn;
        make_note_preview = true;
      }
    }
  }
  m_edit_state.mouse_edit_state.type = type;

  if (make_note_preview) {
    auto maybe_unit_no =
        m_controller->unitIdMap().idToNo(m_edit_state.m_current_unit_id);
    if (maybe_unit_no != std::nullopt) {
      qint32 vel = m_edit_state.mouse_edit_state.base_velocity;

      int pitch = m_edit_state.mouse_edit_state.current_pitch;
      pitch = quantize(pitch, m_edit_state.m_quantize_pitch) +
              m_edit_state.m_quantize_pitch;
      m_edit_state.mouse_edit_state.last_pitch = pitch;

      int clock = m_edit_state.mouse_edit_state.current_clock;
      clock = quantize(clock, m_edit_state.m_quantize_clock);

      m_audio_note_preview = std::make_unique<NotePreview>(
          m_pxtn, &m_moo_state->params, maybe_unit_no.value(), clock, pitch,
          vel, this);
    }
  }
  emit editStateChanged();
}

void KeyboardView::mouseMoveEvent(QMouseEvent *event) {
  if (m_audio_note_preview != nullptr)
    m_audio_note_preview->setVel(
        impliedVelocity(m_edit_state.mouse_edit_state, m_edit_state.scale));

  updateStatePositions(m_edit_state, event);
  emit editStateChanged();
  event->ignore();
}

void KeyboardView::cycleCurrentUnit(int offset) {
  switch (m_edit_state.mouse_edit_state.type) {
    case MouseEditState::Type::SetOn:
    case MouseEditState::Type::DeleteOn:
    case MouseEditState::Type::SetNote:
    case MouseEditState::Type::DeleteNote:
    case MouseEditState::Type::Select:
      break;
    case MouseEditState::Type::Nothing:
    case MouseEditState::Type::Seek:
      auto maybe_unit_no =
          m_controller->unitIdMap().idToNo(m_edit_state.m_current_unit_id);
      if (maybe_unit_no == std::nullopt) break;
      qint32 unit_no = nonnegative_modulo(maybe_unit_no.value() + offset,
                                          m_pxtn->Unit_Num());
      // qDebug() << "Changing unit id" << m_edit_state.m_current_unit_id
      //         << m_sync->unitIdMap().noToId(unit_no);
      m_edit_state.m_current_unit_id =
          m_controller->unitIdMap().noToId(unit_no);
      emit currentUnitNoChanged(unit_no);
      emit editStateChanged();
      break;
  }
}

void KeyboardView::setCurrentUnitNo(int unit_no) {
  // qDebug() << "Changing unit id" << m_edit_state.m_current_unit_id
  //         << m_sync->unitIdMap().noToId(unit_no);
  m_edit_state.m_current_unit_id = m_controller->unitIdMap().noToId(unit_no);

  emit editStateChanged();
}

void KeyboardView::clearRemoteEditStates() {
  m_remote_edit_states.clear();

  emit userListChanged(getUserList(m_remote_edit_states));
}

void KeyboardView::loadHistory(QList<ServerAction> &history) {
  for (ServerAction &a : history) processRemoteAction(a);
}

void KeyboardView::setUid(qint64 uid) { m_controller->setUid(uid); }

void KeyboardView::resetUnitIdMap() { m_controller->resetUnitIdMap(); }

void KeyboardView::refreshQuantSettings() {
  m_edit_state.m_quantize_clock =
      m_pxtn->master->get_beat_clock() / quantizeXOptions[quantXIndex].second;
  m_edit_state.m_quantize_pitch =
      PITCH_PER_KEY / quantizeYOptions[quantizeSelectionY].second;
}

void KeyboardView::setQuantXIndex(int q) {
  quantXIndex = q;
  refreshQuantSettings();
  emit editStateChanged();
}
void KeyboardView::setQuantYIndex(int q) {
  quantizeSelectionY = q;
  refreshQuantSettings();
  emit editStateChanged();
}
void KeyboardView::seekPosition(int clock) {
  seekMoo(m_pxtn, m_moo_state, clock);
  m_this_seek = clock;
  m_this_seek_caught_up = false;
}

void KeyboardView::selectAll() {
  m_edit_state.mouse_edit_state.selection.emplace(
      Interval{0, m_pxtn->master->get_clock_num()});
  emit editStateChanged();
}

void KeyboardView::deselect() {
  m_edit_state.mouse_edit_state.selection.reset();
}

void KeyboardView::transposeSelection(Direction dir, bool wide, bool shift) {
  if (m_edit_state.mouse_edit_state.selection.has_value()) {
    int offset;
    switch (dir) {
      case Direction::UP:
        offset = 1;
        break;
      case Direction::DOWN:
        offset = -1;
        break;
    }
    EVENTKIND kind;
    if (shift) {
      kind = EVENTKIND_KEY;
      offset *= PITCH_PER_KEY * (wide ? 12 : 1);
    } else {
      kind = EVENTKIND_VELOCITY;
      offset *= (wide ? 16 : 4);
    }
    Interval interval(m_edit_state.mouse_edit_state.selection.value());

    using namespace Action;
    std::list<Primitive> as;
    for (qint32 unit : selectedUnitNos()) {
      if (kind != EVENTKIND_VELOCITY) {
        int32_t start_val =
            m_pxtn->evels->get_Value(interval.start, unit, kind);
        as.push_back({kind, unit, interval.start, Delete{interval.start + 1}});
        as.push_back({kind, unit, interval.start, Add{start_val}});
      }
      as.push_back({kind, unit, interval.start, Shift{interval.end, offset}});
      if (kind != EVENTKIND_VELOCITY &&
          interval.end < m_pxtn->master->get_clock_num()) {
        int32_t end_val = m_pxtn->evels->get_Value(interval.end, unit, kind);
        as.push_back({kind, unit, interval.end, Delete{interval.end + 1}});
        as.push_back({kind, unit, interval.end, Add{end_val}});
      }
    }
    m_client->sendAction(m_controller->applyLocalAction(as));
  }
}

void KeyboardView::mouseReleaseEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }
  if (m_audio_note_preview) m_audio_note_preview = nullptr;
  Interval clock_int(
      m_edit_state.mouse_edit_state.clock_int(m_edit_state.m_quantize_clock));
  int start_pitch = quantize(m_edit_state.mouse_edit_state.start_pitch,
                             m_edit_state.m_quantize_pitch) +
                    m_edit_state.m_quantize_pitch;
  // int end_pitch = int(round(pitchOfY(event->localPos().y())));

  if (m_pxtn->Unit_Num() > 0) {
    using namespace Action;
    std::list<Primitive> actions;
    switch (m_edit_state.mouse_edit_state.type) {
      case MouseEditState::SetOn:
      case MouseEditState::DeleteOn:
        actions.push_back({EVENTKIND_ON, m_edit_state.m_current_unit_id,
                           clock_int.start, Delete{clock_int.end}});
        actions.push_back({EVENTKIND_VELOCITY, m_edit_state.m_current_unit_id,
                           clock_int.start, Delete{clock_int.end}});
        actions.push_back({EVENTKIND_KEY, m_edit_state.m_current_unit_id,
                           clock_int.start, Delete{clock_int.end}});
        if (m_edit_state.mouse_edit_state.type == MouseEditState::SetOn) {
          actions.push_back({EVENTKIND_ON, m_edit_state.m_current_unit_id,
                             clock_int.start, Add{clock_int.length()}});
          qint32 vel = impliedVelocity(m_edit_state.mouse_edit_state,
                                       m_edit_state.scale);
          m_edit_state.mouse_edit_state.base_velocity = vel;
          actions.push_back({EVENTKIND_VELOCITY, m_edit_state.m_current_unit_id,
                             clock_int.start, Add{vel}});
          actions.push_back({EVENTKIND_KEY, m_edit_state.m_current_unit_id,
                             clock_int.start, Add{start_pitch}});
        }
        break;
      case MouseEditState::SetNote:
      case MouseEditState::DeleteNote:
        actions.push_back({EVENTKIND_KEY, m_edit_state.m_current_unit_id,
                           clock_int.start, Delete{clock_int.end}});

        if (m_edit_state.mouse_edit_state.type == MouseEditState::SetNote)
          actions.push_back({EVENTKIND_KEY, m_edit_state.m_current_unit_id,
                             clock_int.start, Add{start_pitch}});
        break;
      case MouseEditState::Seek:
        if (event->button() & Qt::LeftButton)
          seekPosition(m_edit_state.mouse_edit_state.current_clock);
        break;
      case MouseEditState::Select:
        m_edit_state.mouse_edit_state.selection.emplace(clock_int);
        break;
      case MouseEditState::Nothing:
        break;
    }
    if (actions.size() > 0) {
      m_client->sendAction(m_controller->applyLocalAction(actions));
      // TODO: Change this to like, when the synchronizer receives an
      // action.
      emit onEdit();
    }
  }
  m_edit_state.mouse_edit_state.type = MouseEditState::Type::Nothing;
  updateStatePositions(m_edit_state, event);
  emit editStateChanged();
  // TODO: maybe would be good to set it up so that edit state change is
  // bundled with the actual remote action
}

void KeyboardView::removeCurrentUnit() {
  if (m_pxtn->Unit_Num() > 0)
    m_client->sendAction(RemoveUnit{m_edit_state.m_current_unit_id});
}

std::set<int> KeyboardView::selectedUnitNos() {
  std::set<int> unit_nos;
  auto unit_no =
      m_controller->unitIdMap().idToNo(m_edit_state.m_current_unit_id);
  if (unit_no.has_value()) unit_nos.insert(unit_no.value());

  for (int i = 0; i < m_pxtn->Unit_Num(); ++i)
    if (m_pxtn->Unit_Get(i)->get_operated()) unit_nos.insert(i);
  return unit_nos;
}

void KeyboardView::copySelection() {
  if (!m_edit_state.mouse_edit_state.selection.has_value()) return;
  m_clipboard.copy(selectedUnitNos(),
                   m_edit_state.mouse_edit_state.selection.value());
}

void KeyboardView::clearSelection() {
  if (!m_edit_state.mouse_edit_state.selection.has_value()) return;
  std::list<Action::Primitive> actions = m_clipboard.makeClear(
      selectedUnitNos(), m_edit_state.mouse_edit_state.selection.value(),
      m_controller->unitIdMap());
  if (actions.size() > 0) {
    m_client->sendAction(m_controller->applyLocalAction(actions));
    emit onEdit();
  }
}

void KeyboardView::cutSelection() {
  copySelection();
  clearSelection();
}

void KeyboardView::paste() {
  if (!m_edit_state.mouse_edit_state.selection.has_value()) return;
  Interval &selection = m_edit_state.mouse_edit_state.selection.value();
  std::list<Action::Primitive> actions = m_clipboard.makePaste(
      selectedUnitNos(), selection.start, m_controller->unitIdMap());
  if (actions.size() > 0) {
    m_client->sendAction(m_controller->applyLocalAction(actions));
    emit onEdit();
  }
  selection.end = selection.start + m_clipboard.copyLength();
  emit editStateChanged();
}
