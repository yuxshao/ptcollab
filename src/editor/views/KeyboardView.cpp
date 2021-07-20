#include "KeyboardView.h"

#include <QDebug>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QTime>

#include "ViewHelper.h"
#include "editor/ComboOptions.h"
#include "editor/Settings.h"
#include "editor/audio/PxtoneUnitIODevice.h"

void LocalEditState::update(const pxtnService *pxtn, const EditState &s) {
  // TODO: dedup from pxtoneClient. maybe
  m_quantize_clock = pxtn->master->get_beat_clock() /
                     quantizeXOptions[s.m_quantize_clock_idx].second;
  m_quantize_pitch =
      PITCH_PER_KEY / quantizeYOptions[s.m_quantize_pitch_idx].second;
  scale = s.scale;
}

QSize KeyboardView::sizeHint() const {
  return QSize(
      one_over_last_clock(m_pxtn) / m_client->editState().scale.clockPerPx,
      m_client->editState().scale.pitchToY(EVENTMIN_KEY));
}

KeyboardView::KeyboardView(PxtoneClient *client, MooClock *moo_clock,
                           QScrollArea *parent)
    : QWidget(parent),
      m_pxtn(client->pxtn()),
      m_timer(new QElapsedTimer),
      painted(0),
      m_dark(false),
      m_edit_state(m_pxtn, client->editState()),
      m_audio_note_preview(nullptr),
      m_anim(new Animation(this)),
      m_client(client),
      m_moo_clock(moo_clock),
      m_test_activity(false) {
  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  updateGeometry();
  setMouseTracking(true);
  m_timer->restart();
  connect(m_anim, &Animation::nextFrame, [this]() { update(); });
  connect(m_anim, &Animation::nextFrame, [this]() {
    // This is not part of paintEvent because it causes some widgets to get
    // rendered outside their viewport, prob. because it causes a repaint in a
    // paintEvent.
    if (!m_client->isPlaying()) return;
    switch (m_client->editState().m_follow_playhead) {
      case FollowPlayhead::None:
        break;
      case FollowPlayhead::Jump:
      case FollowPlayhead::Follow:
        ensurePlayheadFollowed();
        break;
    }
  });

  connect(m_client, &PxtoneClient::editStateChanged,
          [this](const EditState &s) {
            if (!(m_edit_state.scale == s.scale)) updateGeometry();
            m_edit_state.update(m_pxtn, s);
          });
  connect(m_client->controller(), &PxtoneController::measureNumChanged, this,
          &QWidget::updateGeometry);
}

void KeyboardView::ensurePlayheadFollowed() {
  emit ensureVisibleX(
      m_moo_clock->now() / m_client->editState().scale.clockPerPx,
      m_client->editState().m_follow_playhead == FollowPlayhead::Follow);
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

int pixelsPerVelocity = 3;
static double slack = 50;
int impliedVelocity(MouseEditState state, const Scale &scale) {
  double delta = 0;
  // Using VelocityDrag here instead of the mouse edit state means if someone
  // has a different setting then during preview it'll incorrectly look like
  // they have your setting. I think this is fine though
  if (std::holds_alternative<MouseKeyboardEdit>(state.kind) &&
      Settings::VelocityDrag::get()) {
    const auto &s = std::get<MouseKeyboardEdit>(state.kind);
    delta = (s.current_pitch - s.start_pitch) / scale.pitchPerPx;
    // Apply a sigmoid so that small changes in y hardly do anything
    delta = 2 * slack / (1 + exp(2 * delta / slack)) + delta - slack;
  }

  return clamp(int(round(state.base_velocity + delta / pixelsPerVelocity)), 0,
               EVENTMAX_VELOCITY);
}
static qint32 arbitrarily_tall = 512;

void drawVelTooltip(QPainter &painter, qint32 vel, qint32 clock, qint32 pitch,
                    const Brush &brush, const Scale &scale, qint32 alpha) {
  if (alpha == 0) return;
  qint32 draw_vel = (EVENTMAX_VELOCITY + vel) / 2;
  painter.setPen(brush.toQColor(draw_vel, true, alpha));
  painter.setFont(QFont("Sans serif", Settings::TextSize::get()));
  painter.drawText(clock / scale.clockPerPx,
                   scale.pitchToY(pitch) - arbitrarily_tall, arbitrarily_tall,
                   arbitrarily_tall, Qt::AlignBottom, QString("%1").arg(vel));
}

void drawGhostOnNote(QPainter &painter, const Interval &interval,
                     const Scale &scale, int width, const Brush &brush,
                     int velocity, int alpha, double alphaMultiplier,
                     bool rowHighlight, int pitch) {
  if (rowHighlight)
    paintBlock(
        pitch, Interval{0, int(scale.clockPerPx * width)}, painter,
        brush.toQColor(128, false, 48 * velocity / 128 * alphaMultiplier),
        scale);
  paintBlock(pitch, interval, painter,
             brush.toQColor(velocity, false, alpha * alphaMultiplier), scale);

  paintHighlight(pitch, std::min(interval.start, interval.end), painter,
                 brush.toQColor(128, true, alpha * alphaMultiplier), scale);
}

void drawOngoingAction(const EditState &state, const LocalEditState &localState,
                       QPainter &painter, int width, int height,
                       std::optional<int> nowNoWrap, const pxtnMaster *master,
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
      if (!std::holds_alternative<MouseKeyboardEdit>(mouse_edit_state.kind))
        break;
      const auto &keyboard_edit_state =
          std::get<MouseKeyboardEdit>(mouse_edit_state.kind);
      int velocity = impliedVelocity(mouse_edit_state, state.scale);
      // TODO: maybe factor out this quantization logic
      Interval interval(
          mouse_edit_state.clock_int(localState.m_quantize_clock));
      int pitch = quantize(keyboard_edit_state.start_pitch,
                           localState.m_quantize_pitch) +
                  localState.m_quantize_pitch;
      int alpha =
          (mouse_edit_state.type == MouseEditState::Nothing ? 128 : 255);

      bool rowHighlight = (mouse_edit_state.type != MouseEditState::Nothing);
      drawGhostOnNote(painter, interval, state.scale, width, brush, velocity,
                      alpha, alphaMultiplier, rowHighlight, pitch);

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
      Interval interval(
          mouse_edit_state.clock_int(localState.m_quantize_clock) /
          state.scale.clockPerPx);
      drawSelection(painter, interval, height, selectionAlphaMultiplier);
    } break;
  }

  if (state.m_input_state.has_value() && nowNoWrap.has_value()) {
    const Input::State::On &v = state.m_input_state.value();

    for (const Interval &interval : v.clock_ints(nowNoWrap.value(), master))
      drawGhostOnNote(painter, interval, state.scale, width, brush, v.on.vel,
                      255, alphaMultiplier, true, v.on.key);
  }
}

static void drawCursor(const EditState &state, QPainter &painter,
                       const QColor &color, const QString &username,
                       qint64 uid) {
  if (!std::holds_alternative<MouseKeyboardEdit>(state.mouse_edit_state.kind))
    return;
  const auto &keyboard_edit_state =
      std::get<MouseKeyboardEdit>(state.mouse_edit_state.kind);
  QPoint position(state.mouse_edit_state.current_clock / state.scale.clockPerPx,
                  state.scale.pitchToY(keyboard_edit_state.current_pitch));
  drawCursor(position, painter, color, username, uid);
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
                      const MouseEditState &mouse, bool drawTooltip, bool muted,
                      int width) {
  Interval on = state.ongoingOnEvent.value();
  Interval interval = interval_intersect(on, segment);
  bool playing = on.contains(current_clock);
  bool firstBlock = interval.start == on.start;
  if (playing && firstBlock && !muted && alpha > 0)
    paintBlock(state.pitch.value, Interval{0, int(scale.clockPerPx * width)},
               painter,
               brush.toQColor(
                   128, false,
                   16 * state.velocity.value / 128 * (alpha / 2 + 128) / 256),
               scale);
  if (interval_intersect(interval, bounds).empty()) return;
  QColor color = brush.toQColor(state.velocity.value, playing && !muted, alpha);
  if (muted)
    color.setHsl(0, color.saturation() * 0.3, color.lightness(), color.alpha());
  paintBlock(state.pitch.value, interval, painter, color, scale);
  if (firstBlock) {
    paintHighlight(state.pitch.value, interval.start, painter,
                   brush.toQColor(255, true, alpha), scale);
    if (drawTooltip) {
      double alphaMultiplier = 0;
      if (std::holds_alternative<MouseKeyboardEdit>(mouse.kind)) {
        alphaMultiplier +=
            smoothDistance(
                (mouse.current_clock - on.start) / 40.0 / scale.clockPerPx,
                (std::get<MouseKeyboardEdit>(mouse.kind).current_pitch -
                 state.pitch.value) /
                    200.0 / scale.pitchPerPx) *
            0.4;
      }
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

void KeyboardView::paintEvent(QPaintEvent *event) {
  ++painted;
  // if (painted > 10) return;
  QPainter painter(this);
  Interval clockBounds = {
      qint32(event->rect().left() * m_client->editState().scale.clockPerPx) -
          WINDOW_BOUND_SLACK,
      qint32(event->rect().right() * m_client->editState().scale.clockPerPx) +
          WINDOW_BOUND_SLACK};

  painter.fillRect(0, 0, size().width(), size().height(), Qt::black);
  // Draw white lines under background
  QBrush beatBrush(QColor::fromRgb(128, 128, 128));
  QBrush measureBrush(Qt::white);
  for (int beat = 0; true; ++beat) {
    bool isMeasureLine = (beat % m_pxtn->master->get_beat_num() == 0);
    int x = m_pxtn->master->get_beat_clock() * beat /
            m_client->editState().scale.clockPerPx;
    if (x > size().width()) break;
    painter.fillRect(x, 0, 1, size().height(),
                     (isMeasureLine ? measureBrush : beatBrush));
  }
  // Draw key background
  QBrush rootNoteBrush(QColor::fromRgb(84, 76, 76));
  QBrush whiteNoteBrush(QColor::fromRgb(64, 64, 64));
  QBrush blackNoteBrush(QColor::fromRgb(32, 32, 32));
  QBrush black(Qt::black);
  for (int row = 0; true; ++row) {
    QBrush *brush;

    if (m_dark)
      brush = &black;
    else {
      if (row == 39)
        brush = &rootNoteBrush;
      else
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
    }

    if (row * PITCH_PER_KEY / m_client->editState().scale.pitchPerPx >
        size().height())
      break;
    int this_y = row * PITCH_PER_KEY / m_client->editState().scale.pitchPerPx;
    // Because of rounding error, calculate height by subbing next from this
    int next_y =
        (row + 1) * PITCH_PER_KEY / m_client->editState().scale.pitchPerPx;
    int h = next_y - this_y - 1;
    if (m_dark && row % 2 == 1) h += 1;
    painter.fillRect(0, this_y, size().width(), h, *brush);
  }

  // Draw FPS
  QPen pen;
  pen.setBrush(Qt::white);
  painter.setPen(pen);
  {
    int elapsed = m_timer->elapsed();
    if (elapsed >= 2000) {
      m_timer->restart();
      int fps = painted * 1000 / elapsed;
      painted = 0;
      emit fpsUpdated(fps);
    }
  }

  // Set up drawing structures that we'll use while iterating through events
  std::vector<DrawState> drawStates;
  for (int i = 0; i < m_pxtn->Unit_Num(); ++i) drawStates.emplace_back();

  painter.setPen(Qt::blue);

  int clock = m_moo_clock->now();

  // Draw the note blocks! Upon hitting an event, see if we are able to draw a
  // previous block.
  // TODO: Draw the current unit at the top.
  std::optional<Interval> selection = std::nullopt;
  std::set<int> selected_unit_nos = selectedUnitNos();
  if (m_dark)
    painter.setCompositionMode(QPainter::CompositionMode_Plus);
  else
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // This is used to draw the current instrument above everything else.
  QPixmap activeLayer(event->rect().size());
  activeLayer.fill(Qt::transparent);
  QPainter activePainter(&activeLayer);
  activePainter.translate(-event->rect().topLeft());

  if (m_client->editState().mouse_edit_state.selection.has_value())
    selection = m_client->editState().mouse_edit_state.selection.value();
  for (const EVERECORD *e = m_pxtn->evels->get_Records(); e != nullptr;
       e = e->next) {
    // if (e->clock > clockBounds.end) break;
    int unit_no = e->unit_no;
    qint32 unit_id = m_client->unitIdMap().noToId(unit_no);
    DrawState &state = drawStates[unit_no];
    const Brush &brush = brushes[unit_id % NUM_BRUSHES];
    bool matchingUnit = (unit_id == m_client->editState().m_current_unit_id);
    QPainter &thisPainter = matchingUnit ? activePainter : painter;
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
          drawStateSegment(thisPainter, state, {state.pitch.clock, e->clock},
                           thisSelection, clockBounds, brush, alpha,
                           m_client->editState().scale, clock,
                           m_client->editState().mouse_edit_state, matchingUnit,
                           muted, width());

        state.ongoingOnEvent.emplace(Interval{e->clock, e->value + e->clock});
        break;
      case EVENTKIND_VELOCITY:
        state.velocity.set(e);
        break;
      case EVENTKIND_KEY:
        // Maybe draw the previous segment of the current on event.
        if (state.ongoingOnEvent.has_value()) {
          drawStateSegment(thisPainter, state, {state.pitch.clock, e->clock},
                           thisSelection, clockBounds, brush, alpha,
                           m_client->editState().scale, clock,
                           m_client->editState().mouse_edit_state, matchingUnit,
                           muted, width());
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
      qint32 unit_id = m_client->unitIdMap().noToId(unit_no);
      DrawState &state = drawStates[unit_no];
      const Brush &brush = brushes[unit_id % NUM_BRUSHES];
      bool matchingUnit = (unit_id == m_client->editState().m_current_unit_id);
      QPainter &thisPainter = matchingUnit ? activePainter : painter;
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
          thisPainter, state,
          {state.pitch.clock, state.ongoingOnEvent.value().end}, thisSelection,
          clockBounds, brush, alpha, m_client->editState().scale, clock,
          m_client->editState().mouse_edit_state, matchingUnit, muted, width());

      state.ongoingOnEvent.reset();
    }
  }
  painter.drawPixmap(event->rect(), activeLayer, activeLayer.rect());

  // Draw selections & ongoing edits / selections / seeks
  for (const auto &[uid, remote_state] : m_client->remoteEditStates()) {
    if (uid == m_client->following_uid() || uid == m_client->uid()) continue;
    if (remote_state.state.has_value()) {
      EditState adjusted_state(remote_state.state.value());
      bool same_unit = adjusted_state.m_current_unit_id ==
                       m_client->editState().m_current_unit_id;
      double alphaMultiplier = (same_unit ? 0.7 : 0.3);
      double selectionAlphaMultiplier = (same_unit ? 0.5 : 0.3);
      adjusted_state.scale = m_client->editState().scale;

      // TODO: colour other's existing selections (or identify somehow. maybe
      // name tag?)
      drawExistingSelection(painter, adjusted_state.mouse_edit_state,
                            adjusted_state.scale.clockPerPx, height(),
                            selectionAlphaMultiplier);
      drawOngoingAction(adjusted_state, LocalEditState(m_pxtn, adjusted_state),
                        painter, width(), height(), std::nullopt,
                        m_pxtn->master, alphaMultiplier,
                        selectionAlphaMultiplier);
    }
  }
  drawExistingSelection(painter, m_client->editState().mouse_edit_state,
                        m_client->editState().scale.clockPerPx, size().height(),
                        1);
  drawOngoingAction(m_client->editState(), m_edit_state, painter, width(),
                    height(), m_moo_clock->nowNoWrap(), m_pxtn->master, 1, 1);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // Draw cursors
  for (const auto &[uid, remote_state] : m_client->remoteEditStates()) {
    if (uid == m_client->following_uid() || uid == m_client->uid()) continue;
    if (remote_state.state.has_value()) {
      EditState state = remote_state.state.value();
      state.scale =
          m_client->editState().scale;  // Position according to our scale
      int unit_id = state.m_current_unit_id;
      // Draw cursor
      QColor color = Qt::white;
      if (unit_id != m_client->editState().m_current_unit_id)
        color = brushes[unit_id % NUM_BRUSHES].toQColor(EVENTMAX_VELOCITY,
                                                        false, 128);
      drawCursor(state, painter, color, remote_state.user, uid);
    }
  }
  {
    QString my_username = "";
    auto it = m_client->remoteEditStates().find(m_client->following_uid());
    if (it != m_client->remoteEditStates().end()) my_username = it->second.user;
    drawCursor(m_client->editState(), painter, Qt::white, my_username,
               m_client->following_uid());
  }

  drawLastSeek(painter, m_client, height(), false);
  drawCurrentPlayerPosition(painter, m_moo_clock, height(),
                            m_client->editState().scale.clockPerPx, false);
  drawRepeatAndEndBars(painter, m_moo_clock,
                       m_client->editState().scale.clockPerPx, height());

  // Simulate activity on a client
  if (m_test_activity) {
    m_client->changeEditState(
        [&](EditState &e) {
          double period = 60;
          double amp = 20;
          e.mouse_edit_state.current_clock += amp * sin(painted / period);
          if (std::holds_alternative<MouseKeyboardEdit>(
                  e.mouse_edit_state.kind))
            std::get<MouseKeyboardEdit>(e.mouse_edit_state.kind)
                .current_pitch += amp * cos(painted / period);
        },
        false);
  }
}

void KeyboardView::wheelEvent(QWheelEvent *event) {
  handleWheelEventWithModifier(event, m_client);
  if (event->isAccepted()) return;

  if (m_client->editState().mouse_edit_state.type == MouseEditState::SetOn &&
      !(event->modifiers() & Qt::ShiftModifier)) {
    m_client->changeEditState(
        [&](EditState &e) {
          auto &vel = e.mouse_edit_state.base_velocity;
          vel = clamp<double>(vel + event->angleDelta().y() * 8.0 / 120, 0,
                              EVENTMAX_VELOCITY);
          if (m_audio_note_preview != nullptr)
            m_audio_note_preview->processEvent(
                EVENTKIND_VELOCITY,
                impliedVelocity(e.mouse_edit_state, e.scale));
        },
        false);
    event->accept();
  }
}

static void updateStatePositions(EditState &edit_state,
                                 const QMouseEvent *event) {
  MouseEditState &state = edit_state.mouse_edit_state;
  state.current_clock =
      std::max(0., event->localPos().x() * edit_state.scale.clockPerPx);
  qint32 current_pitch = edit_state.scale.pitchOfY(event->localPos().y());
  if (!std::holds_alternative<MouseKeyboardEdit>(state.kind))
    state.kind.emplace<MouseKeyboardEdit>(
        MouseKeyboardEdit{current_pitch, current_pitch});
  auto &keyboard_edit_state = std::get<MouseKeyboardEdit>(state.kind);

  keyboard_edit_state.current_pitch = current_pitch;

  if (state.type == MouseEditState::Type::Nothing ||
      state.type == MouseEditState::Type::Seek) {
    state.type = (event->modifiers() & Qt::ShiftModifier
                      ? MouseEditState::Type::Seek
                      : MouseEditState::Type::Nothing);
    state.start_clock = state.current_clock;
    keyboard_edit_state.start_pitch = current_pitch;
  }
}

void KeyboardView::mousePressEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }

  if (m_pxtn->Unit_Num() == 0) return;
  m_client->changeEditState(
      [&](EditState &s) {
        updateStatePositions(s, event);

        bool make_note_preview = false;
        MouseEditState::Type type;
        if (event->modifiers() & Qt::ShiftModifier) {
          if (event->modifiers() & Qt::ControlModifier &&
              event->button() != Qt::RightButton)
            type = MouseEditState::Type::Select;
          else {
            if (event->button() == Qt::RightButton)
              s.mouse_edit_state.selection.reset();
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
        s.mouse_edit_state.type = type;

        if (make_note_preview) {
          auto maybe_unit_no = m_client->unitIdMap().idToNo(
              m_client->editState().m_current_unit_id);
          if (maybe_unit_no != std::nullopt &&
              std::holds_alternative<MouseKeyboardEdit>(
                  s.mouse_edit_state.kind)) {
            int pitch = std::get<MouseKeyboardEdit>(s.mouse_edit_state.kind)
                            .current_pitch;
            pitch = quantize(pitch, m_edit_state.m_quantize_pitch) +
                    m_edit_state.m_quantize_pitch;
            s.mouse_edit_state.last_pitch = pitch;

            int clock = m_client->editState().mouse_edit_state.current_clock;
            clock = quantize(clock, m_edit_state.m_quantize_clock);

            qint32 unit_no = maybe_unit_no.value();
            qint32 vel =
                m_pxtn->evels->get_Value(clock, unit_no, EVENTKIND_VELOCITY);
            s.mouse_edit_state.base_velocity = vel;

            m_audio_note_preview = std::make_unique<NotePreview>(
                m_pxtn, &m_client->moo()->params, unit_no, clock, pitch, vel,
                m_client->audioState()->bufferSize(),
                Settings::ChordPreview::get(), this);
          }
        }
      },
      true);
}

void KeyboardView::mouseMoveEvent(QMouseEvent *event) {
  if (m_audio_note_preview != nullptr)
    m_audio_note_preview->processEvent(
        EVENTKIND_VELOCITY,
        impliedVelocity(m_client->editState().mouse_edit_state,
                        m_client->editState().scale));

  if (!m_client->isFollowing())
    m_client->changeEditState([&](auto &s) { updateStatePositions(s, event); },
                              true);
  event->ignore();
}

static bool canChangeUnit(PxtoneClient *client) {
  switch (client->editState().mouse_edit_state.type) {
    case MouseEditState::Type::SetOn:
    case MouseEditState::Type::DeleteOn:
    case MouseEditState::Type::SetNote:
    case MouseEditState::Type::DeleteNote:
    case MouseEditState::Type::Select:
      return false;
    case MouseEditState::Type::Nothing:
    case MouseEditState::Type::Seek:
      return true;
  }
  return false;
}

void KeyboardView::cycleCurrentUnit(int offset) {
  if (!canChangeUnit(m_client)) return;

  auto maybe_unit_no =
      m_client->unitIdMap().idToNo(m_client->editState().m_current_unit_id);
  if (maybe_unit_no == std::nullopt) return;
  qint32 unit_no =
      nonnegative_modulo(maybe_unit_no.value() + offset, m_pxtn->Unit_Num());
  qint32 unit_id = m_client->unitIdMap().noToId(unit_no);
  m_client->changeEditState([&](auto &s) { s.m_current_unit_id = unit_id; },
                            false);
}

void KeyboardView::setCurrentUnitNo(int unit_no, bool preserve_follow) {
  if (!canChangeUnit(m_client)) return;
  if (unit_no >= m_pxtn->Unit_Num()) return;
  qint32 unit_id = m_client->unitIdMap().noToId(unit_no);
  m_client->changeEditState([&](auto &s) { s.m_current_unit_id = unit_id; },
                            preserve_follow);
}

void KeyboardView::selectAll(bool preserveFollow) {
  m_client->changeEditState(
      [&](EditState &s) {
        s.mouse_edit_state.selection.emplace(
            Interval{0, m_pxtn->master->get_clock_num()});
      },
      preserveFollow);
}

void KeyboardView::transposeSelection(Direction dir, bool wide, bool shift) {
  if (m_client->editState().mouse_edit_state.selection.has_value()) {
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
      EVENTKIND current_kind =
          paramOptions[m_client->editState().m_current_param_kind_idx].second;
      switch (current_kind) {
        case EVENTKIND_KEY:
        case EVENTKIND_PAN_VOLUME:
        case EVENTKIND_VELOCITY:
        case EVENTKIND_VOLUME:
        case EVENTKIND_PAN_TIME:
          kind = current_kind;
          break;
        default:
          kind = EVENTKIND_VELOCITY;
          break;
      }
      offset *= (wide ? 16 : 4);
    }
    Interval interval(m_client->editState().mouse_edit_state.selection.value());

    using namespace Action;
    std::list<Primitive> as;
    for (qint32 unitNo : selectedUnitNos()) {
      qint32 unit = m_client->unitIdMap().noToId(unitNo);
      if (kind != EVENTKIND_VELOCITY) {
        int32_t start_val =
            m_pxtn->evels->get_Value(interval.start, unitNo, kind);
        as.push_back({kind, unit, interval.start, Delete{interval.start + 1}});
        as.push_back({kind, unit, interval.start, Add{start_val}});
      }
      as.push_back({kind, unit, interval.start, Shift{interval.end, offset}});
      if (kind != EVENTKIND_VELOCITY &&
          interval.end < m_pxtn->master->get_clock_num()) {
        int32_t end_val = m_pxtn->evels->get_Value(interval.end, unitNo, kind);
        as.push_back({kind, unit, interval.end, Delete{interval.end + 1}});
        as.push_back({kind, unit, interval.end, Add{end_val}});
      }
    }
    m_client->applyAction(as);
  }
}

void KeyboardView::mouseReleaseEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }
  if (m_audio_note_preview) m_audio_note_preview = nullptr;

  const auto &kind = m_client->editState().mouse_edit_state.kind;
  if (!std::holds_alternative<MouseKeyboardEdit>(
          m_client->editState().mouse_edit_state.kind))
    return;
  const auto &keyboard_edit_state = std::get<MouseKeyboardEdit>(kind);

  Interval clock_int(m_client->editState().mouse_edit_state.clock_int(
      m_client->quantizeClock()));
  int start_pitch =
      quantize(keyboard_edit_state.start_pitch, m_edit_state.m_quantize_pitch) +
      m_edit_state.m_quantize_pitch;
  // int end_pitch = int(round(pitchOfY(event->localPos().y())));

  m_client->changeEditState(
      [&](auto &s) {
        if (m_pxtn->Unit_Num() > 0) {
          using namespace Action;
          std::list<Primitive> actions;
          switch (s.mouse_edit_state.type) {
            case MouseEditState::SetOn:
            case MouseEditState::DeleteOn:
              actions.push_back({EVENTKIND_ON,
                                 m_client->editState().m_current_unit_id,
                                 clock_int.start, Delete{clock_int.end}});
              actions.push_back({EVENTKIND_VELOCITY,
                                 m_client->editState().m_current_unit_id,
                                 clock_int.start, Delete{clock_int.end}});
              actions.push_back({EVENTKIND_KEY,
                                 m_client->editState().m_current_unit_id,
                                 clock_int.start, Delete{clock_int.end}});
              if (m_client->editState().mouse_edit_state.type ==
                  MouseEditState::SetOn) {
                actions.push_back({EVENTKIND_ON,
                                   m_client->editState().m_current_unit_id,
                                   clock_int.start, Add{clock_int.length()}});
                qint32 vel =
                    impliedVelocity(m_client->editState().mouse_edit_state,
                                    m_client->editState().scale);
                s.mouse_edit_state.base_velocity = vel;
                actions.push_back({EVENTKIND_VELOCITY,
                                   m_client->editState().m_current_unit_id,
                                   clock_int.start, Add{vel}});
                actions.push_back({EVENTKIND_KEY,
                                   m_client->editState().m_current_unit_id,
                                   clock_int.start, Add{start_pitch}});
              }
              break;
            case MouseEditState::SetNote:
            case MouseEditState::DeleteNote:
              actions.push_back({EVENTKIND_KEY,
                                 m_client->editState().m_current_unit_id,
                                 clock_int.start, Delete{clock_int.end}});

              if (m_client->editState().mouse_edit_state.type ==
                  MouseEditState::SetNote)
                actions.push_back({EVENTKIND_KEY,
                                   m_client->editState().m_current_unit_id,
                                   clock_int.start, Add{start_pitch}});
              break;
            case MouseEditState::Seek:
              if (event->button() & Qt::LeftButton)
                m_client->seekMoo(
                    m_client->editState().mouse_edit_state.current_clock);
              break;
            case MouseEditState::Select:
              s.mouse_edit_state.selection.emplace(clock_int);
              break;
            case MouseEditState::Nothing:
              break;
          }
          if (actions.size() > 0) {
            m_client->applyAction(actions);
          }
        }
        s.mouse_edit_state.type = MouseEditState::Type::Nothing;
        updateStatePositions(s, event);
      },
      false);
}

std::set<int> KeyboardView::selectedUnitNos() {
  std::set<int> unit_nos;
  auto unit_no =
      m_client->unitIdMap().idToNo(m_client->editState().m_current_unit_id);
  if (unit_no.has_value()) unit_nos.insert(unit_no.value());

  for (int i = 0; i < m_pxtn->Unit_Num(); ++i)
    if (m_pxtn->Unit_Get(i)->get_operated()) unit_nos.insert(i);
  return unit_nos;
}

void KeyboardView::copySelection() {
  if (!m_client->editState().mouse_edit_state.selection.has_value()) return;
  m_client->clipboard()->copy(
      selectedUnitNos(),
      m_client->editState().mouse_edit_state.selection.value(), m_pxtn);
}

void KeyboardView::clearSelection() {
  if (!m_client->editState().mouse_edit_state.selection.has_value()) return;
  std::list<Action::Primitive> actions = m_client->clipboard()->makeClear(
      selectedUnitNos(),
      m_client->editState().mouse_edit_state.selection.value(),
      m_client->unitIdMap());
  if (actions.size() > 0) m_client->applyAction(actions);
}

void KeyboardView::cutSelection() {
  copySelection();
  clearSelection();
}

void KeyboardView::quantizeSelection() {
  const static std::set<EVENTKIND> kindsToQuantize(
      {EVENTKIND_VELOCITY, EVENTKIND_KEY, EVENTKIND_ON});
  const Interval &range =
      m_client->editState().mouse_edit_state.selection.value();
  const std::set<int> &unit_nos(selectedUnitNos());
  LocalEditState localState(m_pxtn, m_client->editState());
  int quantizeClock = localState.m_quantize_clock;
  // Quantize with rounding, rather than flooring / ceiling is more useful for
  // MIDI input.
  auto quantize = [&](qint32 c) {
    return ((2 * c + quantizeClock) / (quantizeClock * 2)) * quantizeClock;
  };

  const EVERECORD *e = nullptr;
  for (e = m_pxtn->evels->get_Records(); e; e = e->next)
    if (e->clock >= range.start) break;

  std::list<Action::Primitive> actions;
  for (int unit_no : unit_nos)
    for (EVENTKIND kind : kindsToQuantize) {
      int unit_id = m_client->unitIdMap().noToId(unit_no);
      actions.push_back(
          {kind, unit_id, range.start, Action::Delete{range.end}});
    }
  for (; e && e->clock < range.end; e = e->next) {
    EVENTKIND kind(EVENTKIND(e->kind));
    if (unit_nos.find(e->unit_no) != unit_nos.end() &&
        kindsToQuantize.find(kind) != kindsToQuantize.end()) {
      int unit_id = m_client->unitIdMap().noToId(e->unit_no);
      qint32 start_clock = quantize(e->clock);
      if (Evelist_Kind_IsTail(e->kind)) {
        // We round the end time up. Even though this might cause an add overlap
        // with the next value, this should be okay in terms of interacting with
        // undo, since the undos of both of these is 2 clears. (It takes some
        // effort to explain)
        int v = std::max(quantizeClock, quantize(e->value));
        actions.push_back({kind, unit_id, start_clock, Action::Add{v}});
      } else
        actions.push_back({kind, unit_id, start_clock, Action::Add{e->value}});
    }
  }
  qDebug() << "apply quantize";
  m_client->applyAction(actions);
}

void KeyboardView::paste(bool preserveFollow) {
  if (!m_client->editState().mouse_edit_state.selection.has_value()) return;
  m_client->changeEditState(
      [&](auto &s) {
        Interval &selection = s.mouse_edit_state.selection.value();
        PasteResult paste_result = m_client->clipboard()->makePaste(
            selectedUnitNos(), selection.start, m_client->unitIdMap());
        if (paste_result.actions.size() > 0)
          m_client->applyAction(paste_result.actions);
        selection.end = selection.start + paste_result.length;
      },
      preserveFollow);
}

void KeyboardView::toggleDark() { m_dark = !m_dark; }
