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
#include "editor/StyleEditor.h"

void LocalEditState::update(const pxtnService *pxtn, const EditState &s) {
  // TODO: dedup from pxtoneClient. maybe
  m_quantize_clock = pxtn->master->get_beat_clock() /
                     quantizeXOptions()[s.m_quantize_clock_idx].second;
  m_quantize_pitch = s.m_quantize_pitch_denom;
  scale = s.scale;
}

QSize KeyboardView::sizeHint() const {
  return worldTransform()
      .mapRect(
          QRect(QPoint(-Settings::LeftPianoWidth::get(), 0),
                QPoint(one_over_last_clock(m_client->pxtn()) /
                           m_client->editState().scale.clockPerPx,
                       m_client->editState().scale.pitchToY(EVENTMIN_KEY))))
      .size();
}

void KeyboardView::setHoveredUnitNo(std::optional<int> new_unit_no) {
  if (m_hovered_unit_no != new_unit_no) {
    m_hovered_unit_no = new_unit_no;
    emit hoverUnitNoChanged(m_hovered_unit_no);
  }
}

KeyboardView::KeyboardView(PxtoneClient *client, QScrollArea *parent)
    : QWidget(parent),
      m_scrollarea(parent),
      m_pxtn(client->pxtn()),
      m_timer(new QElapsedTimer),
      painted(0),
      m_dark(false),
      m_edit_state(m_pxtn, client->editState()),
      m_audio_note_preview(nullptr),
      m_anim(new Animation(this)),
      m_client(client),
      m_select_unit_enabled(false),
      m_last_left_kb_pitch(0),
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
  bool follow_exactly =
      m_client->editState().m_follow_playhead == FollowPlayhead::Follow;
  double logicalX =
      m_client->controller()->m_audio_renderer->moo_timing().now_clock /
      m_client->editState().scale.clockPerPx;
  double x = worldTransform().map(QPointF(logicalX, 0)).x();
  emit ensureVisibleX(x, follow_exactly);
}

void KeyboardView::setFocusedUnit(std::optional<int> unit_no) {
  m_focused_unit_no = unit_no;
}

void KeyboardView::setSelectUnitEnabled(bool b) {
  m_select_unit_enabled = b && (m_client->editState().mouse_edit_state.type ==
                                MouseEditState::Type::Nothing);
}

std::map<int, int> &KeyboardView::currentMidiNotes() { return m_midi_notes; }

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

#undef DrawState  // Windows GDI thing
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
                              const Scale &scale, int displayEdo) {
  int rowHeight = PITCH_PER_OCTAVE / scale.pitchPerPx / displayEdo;
  painter.fillRect(clock / scale.clockPerPx,
                   scale.pitchToY(pitch) - rowHeight / 3, widthInPx,
                   rowHeight * 2 / 3, brush);
}

static void drawAtClockPitch(int clock, int pitch, int widthInPx,
                             QPainter &painter, const Scale &scale,
                             int displayEdo) {
  int rowHeight = PITCH_PER_OCTAVE / scale.pitchPerPx / displayEdo;
  painter.drawRect(clock / scale.clockPerPx - 1,
                   scale.pitchToY(pitch) - rowHeight / 3 - 1, widthInPx + 1,
                   rowHeight * 2 / 3 + 1);
}

static void paintBlock(int pitch, const Interval &segment, QPainter &painter,
                       const QBrush &brush, const Scale &scale,
                       int displayEdo) {
  paintAtClockPitch(segment.start, pitch,
                    std::max<int>(1, segment.length() / scale.clockPerPx),
                    painter, brush, scale, displayEdo);
}

static void drawBlock(int pitch, const Interval &segment, QPainter &painter,
                      const Scale &scale, int displayEdo) {
  drawAtClockPitch(segment.start, pitch, segment.length() / scale.clockPerPx,
                   painter, scale, displayEdo);
}

static void paintHighlight(int pitch, int clock, QPainter &painter,
                           const QBrush &brush, const Scale &scale,
                           int displayEdo) {
  paintAtClockPitch(clock, pitch, 2, painter, brush, scale, displayEdo);
}

static qint32 arbitrarily_tall = 512;

void drawVelTooltip(QPainter &painter, qint32 vel, qint32 clock, qint32 pitch,
                    const NoteBrush &brush, const Scale &scale, qint32 alpha,
                    int displayEdo) {
  if (alpha == 0) return;
  qint32 draw_vel = (EVENTMAX_VELOCITY + vel) / 2;
  painter.setPen(brush.toQColor(draw_vel, 1, alpha));
  painter.setFont(
      QFont(StyleEditor::config.font.EditorFont, Settings::TextSize::get()));
  painter.drawText(
      clock / scale.clockPerPx,
      scale.pitchToY(pitch + PITCH_PER_OCTAVE / displayEdo * 2 / 5) -
          arbitrarily_tall,
      arbitrarily_tall, arbitrarily_tall, Qt::AlignBottom,
      QString("%1").arg(vel));
}

void drawGhostOnNote(QPainter &painter, const Interval &interval,
                     const Scale &scale, int width, const NoteBrush &brush,
                     int velocity, int alpha, double alphaMultiplier,
                     bool rowHighlight, bool noteHighlight, int pitch,
                     int displayEdo) {
  if (rowHighlight)
    paintBlock(pitch, Interval{0, int(scale.clockPerPx * width)}, painter,
               brush.toQColor(128, 0, 48 * velocity / 128 * alphaMultiplier),
               scale, displayEdo);
  paintBlock(pitch, interval, painter,
             brush.toQColor(velocity, 0, alpha * alphaMultiplier), scale,
             displayEdo);
  if (noteHighlight)
    paintHighlight(pitch, std::min(interval.start, interval.end), painter,
                   brush.toQColor(128, 1, alpha * alphaMultiplier), scale,
                   displayEdo);
}

struct SetNoteInterval {
  Interval interval;
  int pitch;
};

std::list<SetNoteInterval> vibratoIntervals(const Interval &interval,
                                            int quantizeClock, int startPitch,
                                            int currentPitch) {
  // We could only make one note on event if the intervals are the same as in
  // the line below but it seems a bit inconsistent.
  // if (startPitch == currentPitch) return {{interval, startPitch}};
  std::list<SetNoteInterval> intervals;
  int pitch = startPitch;
  for (int t = interval.start; t < interval.end; t += quantizeClock) {
    intervals.push_back(
        {Interval{t, std::min(t + quantizeClock, interval.end)}, pitch});
    if (pitch == startPitch)
      pitch = currentPitch;
    else
      pitch = startPitch;
  }
  return intervals;
}

int quantize_pitch(long p, long d) {
  // basically snap p to the nearest PITCH_PER_OCTAVE / d. need to handle neg.
  bool neg = p < 0;
  int q =
      ((2 * (neg ? -p : p) * d + PITCH_PER_OCTAVE) / (2 * PITCH_PER_OCTAVE)) *
      PITCH_PER_OCTAVE / d;
  return (neg ? -q : q);
}

void drawOngoingAction(const EditState &state, const LocalEditState &localState,
                       QPainter &painter, int width, int height,
                       std::optional<int> nowNoWrap, const pxtnMaster *master,
                       double alphaMultiplier, double selectionAlphaMultiplier,
                       int displayEdo) {
  const NoteBrush &brush = *StyleEditor::noteBrush(state.m_current_unit_id);
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
      if (!std::holds_alternative<MouseMainKeyboard>(keyboard_edit_state.kind))
        break;
      const auto &[start_pitch] =
          std::get<MouseMainKeyboard>(keyboard_edit_state.kind);

      int velocity = impliedVelocity(mouse_edit_state, state.scale.pitchPerPx);
      // TODO: maybe factor out this quantization logic
      Interval interval(
          mouse_edit_state.clock_int(localState.m_quantize_clock));
      int pitch = quantize_pitch(start_pitch, localState.m_quantize_pitch);
      int alpha =
          (mouse_edit_state.type == MouseEditState::Nothing ? 128 : 255);
      bool drawNoteHighlight =
          (mouse_edit_state.type != MouseEditState::Type::SetNote &&
           mouse_edit_state.type != MouseEditState::DeleteNote);

      if (mouse_edit_state.type == MouseEditState::Type::SetNote) {
        int end_pitch = quantize_pitch(keyboard_edit_state.current_pitch,
                                       localState.m_quantize_pitch);
        std::list<SetNoteInterval> intervals = vibratoIntervals(
            interval, localState.m_quantize_clock, pitch, end_pitch);
        for (const auto &[interval, pitch] : intervals) {
          drawGhostOnNote(painter, interval, state.scale, width, brush,
                          velocity, alpha, alphaMultiplier, false,
                          drawNoteHighlight, pitch, displayEdo);
        }
      } else {
        bool rowHighlight = (mouse_edit_state.type != MouseEditState::Nothing);
        drawGhostOnNote(painter, interval, state.scale, width, brush, velocity,
                        alpha, alphaMultiplier, rowHighlight, drawNoteHighlight,
                        pitch, displayEdo);
      }

      if (mouse_edit_state.type == MouseEditState::SetOn)
        drawVelTooltip(painter, velocity, interval.start, pitch, brush,
                       state.scale, 255 * alphaMultiplier, displayEdo);

    } break;
    case MouseEditState::Type::Seek: {
      QColor color = StyleEditor::config.color.Playhead;
      color.setAlpha(color.alpha() * alphaMultiplier / 2);
      painter.fillRect(mouse_edit_state.current_clock / state.scale.clockPerPx,
                       0, 1, height, color);
      break;
    }
    case MouseEditState::Type::Select: {
      Interval interval(
          mouse_edit_state.clock_int(localState.m_quantize_clock) /
          state.scale.clockPerPx);
      drawSelection(painter, interval, height, selectionAlphaMultiplier);
    } break;
  }

  if (nowNoWrap.has_value()) {
    for (const auto &[unit_id, v] : state.m_input_state.notes_by_id) {
      for (const Interval &interval : v.clock_ints(nowNoWrap.value(), master)) {
        const NoteBrush &brush = *StyleEditor::noteBrush(unit_id);
        drawGhostOnNote(painter, interval, state.scale, width, brush,
                        v.on.vel(), 255, alphaMultiplier, true, true, v.on.key,
                        displayEdo);
      }
    }
  }
}

static void drawCursor(const EditState &state, QPainter &painter,
                       const QColor &color, const QString &username,
                       qint64 uid) {
  std::optional<qint32> pitch;
  if (std::holds_alternative<MouseKeyboardEdit>(state.mouse_edit_state.kind))
    pitch =
        std::get<MouseKeyboardEdit>(state.mouse_edit_state.kind).current_pitch;

  if (!pitch.has_value()) return;
  QPoint position(state.mouse_edit_state.current_clock / state.scale.clockPerPx,
                  state.scale.pitchToY(pitch.value()));
  drawCursor(position, painter, color, username, uid);
}

void drawLeftPiano(QPainter &painter, int y, int h, const QBrush &b,
                   QBrush *bInner) {
  painter.fillRect(0, y, Settings::LeftPianoWidth::get(), h, b);
  painter.fillRect(Settings::LeftPianoWidth::get(), y + 1, 1, h - 2, b);
  if (bInner)
    painter.fillRect(0, y, Settings::LeftPianoWidth::get() * 2 / 3, h, *bInner);
}

double smoothDistance(double dy, double dx) {
  double r = dy * dy + dx * dx;
  return std::max(0.0, 1 / (r + 1));
}
void drawLeftPianoNoteHighlight(QPainter &painter, int pitch,
                                const Scale &scale, int displayEdo,
                                const QColor &c) {
  int nextPitch = pitch - PITCH_PER_OCTAVE / displayEdo;
  int floor_h = PITCH_PER_OCTAVE / scale.pitchPerPx / displayEdo;
  int h = int(scale.pitchToY(nextPitch)) - int(scale.pitchToY(pitch)) - 1;
  drawLeftPiano(painter, int(scale.pitchToY(pitch)) - floor_h / 2, h, c,
                nullptr);
}

struct LeftPianoNote {
  int pitch;
  QColor c;
};

struct NoteSegment {
  Interval interval;
  double on_strength;
  int pitch;
  int velocity;
  Interval on;
};

struct UnitDrawParam {
  std::shared_ptr<const NoteBrush> brush;
  bool matchingUnit;
  bool hoveredUnit;
  bool draw_left_highlights;
  bool visible;
  int alpha;
  bool scale_alpha_by_velocity;
  bool muted;
  bool selected;
};

void drawNoteSegment(QPainter &painter, const UnitDrawParam &param,
                     const NoteSegment &note, const Scale &scale,
                     int displayEdo, const std::optional<Interval> &selection) {
  QColor color = param.brush->toQColor(
      note.velocity, (param.muted ? 0.0 : note.on_strength), param.alpha);
  if (param.muted)
    color.setHsl(0, color.saturation() * 0.3, color.lightness(), color.alpha());
  if (param.scale_alpha_by_velocity)
    color.setAlpha(color.alpha() * note.velocity / EVENTMAX_VELOCITY);

  // draw note
  paintBlock(note.pitch, note.interval, painter, color, scale, displayEdo);

  // draw highlight at front
  if (note.interval.start == note.on.start && param.draw_left_highlights) {
    paintHighlight(note.pitch, note.interval.start, painter,
                   param.brush->toQColor(255, 1, color.alpha()), scale,
                   displayEdo);
  }

  // draw selection outline
  if (selection.has_value() && param.selected) {
    Interval selection_segment =
        interval_intersect(selection.value(), note.interval);
    if (!selection_segment.empty()) {
      painter.setPen(
          param.brush->toQColor(EVENTDEFAULT_VELOCITY, 1, param.alpha));
      drawBlock(note.pitch, selection_segment, painter, scale, displayEdo);
    }
  }
}

void maybeDrawNoteVelTooltip(QPainter &painter, const NoteSegment &note,
                             const UnitDrawParam &param,
                             const MouseEditState &mouse, const Scale &scale,
                             int displayEdo,
                             const std::optional<Interval> &selection) {
  if (note.on.start != note.interval.start) return;
  double alphaMultiplier = 0;
  if (std::holds_alternative<MouseKeyboardEdit>(mouse.kind)) {
    alphaMultiplier +=
        smoothDistance(
            (mouse.current_clock - note.on.start) / 40.0 / scale.clockPerPx,
            (std::get<MouseKeyboardEdit>(mouse.kind).current_pitch -
             note.pitch) /
                200.0 / scale.pitchPerPx) *
        0.4;
  }
  if (mouse.current_clock < note.on.end &&
      mouse.current_clock >= note.on.start &&
      mouse.type != MouseEditState::SetOn)
    alphaMultiplier += 0.6;
  else if (selection.has_value() && selection.value().contains(note.on.start))
    alphaMultiplier += 0.3;
  drawVelTooltip(painter, note.velocity, note.on.start, note.pitch,
                 *param.brush, scale, param.alpha * alphaMultiplier,
                 displayEdo);
}

double distance_to_mouse(const MouseEditState &mouse, const Interval &clock_int,
                         int pitch, int displayEdo, const Scale &scale) {
  double dx =
      distance_to_range(mouse.current_clock, clock_int.start, clock_int.end) /
      scale.clockPerPx;
  int pitch_per_key = PITCH_PER_OCTAVE / displayEdo;
  double dy =
      distance_to_range(std::get<MouseKeyboardEdit>(mouse.kind).current_pitch,
                        pitch - pitch_per_key / 2, pitch + pitch_per_key / 2) /
      scale.pitchPerPx;
  return dx * dx + dy * dy;
}

struct BackgroundKeyRow {
  enum { Root, Black, White } color;
  int y;
  int h;
  int pitch;
};

void KeyboardView::paintEvent(QPaintEvent *raw_event) {
  ++painted;
  QPainter painter(this);
  painter.fillRect(raw_event->rect(), Qt::black);

  painter.setTransform(worldTransform());
  QRectF event_rect_f =
      worldTransform().inverted().mapRect(QRectF(raw_event->rect()));
  QPaintEvent e(worldTransform().inverted().mapRect(raw_event->rect()));
  QPaintEvent *event = &e;

  Interval clockBounds = {
      qint32(event->rect().left() * m_client->editState().scale.clockPerPx) -
          WINDOW_BOUND_SLACK,
      qint32(event->rect().right() * m_client->editState().scale.clockPerPx) +
          WINDOW_BOUND_SLACK};

  // Draw white lines under background
  QBrush beatBrush(StyleEditor::config.color.KeyboardBeat);
  QBrush measureBrush(StyleEditor::config.color.KeyboardMeasure);
  for (int beat = 0; true; ++beat) {
    bool isMeasureLine = (beat % m_pxtn->master->get_beat_num() == 0);
    int x = m_pxtn->master->get_beat_clock() * beat /
            m_client->editState().scale.clockPerPx;
    if (x > size().width()) break;
    painter.fillRect(x, 0, 1, size().height(),
                     (isMeasureLine ? measureBrush : beatBrush));
  }

  // Draw key background
  QColor rootNoteColor = StyleEditor::config.color.KeyboardRootNote;
  QBrush rootNoteBrush = rootNoteColor;
  QBrush whiteNoteBrush = StyleEditor::config.color.KeyboardWhiteNote;
  QBrush blackNoteBrush = StyleEditor::config.color.KeyboardBlackNote;

  QBrush whiteLeftBrush = StyleEditor::config.color.KeyboardWhiteLeft;
  QBrush blackLeftBrush = StyleEditor::config.color.KeyboardBlackLeft;
  QBrush blackLeftInnerBrush = StyleEditor::config.color.KeyboardBlackLeftInner;
  QBrush black = StyleEditor::config.color.KeyboardBlack;

  double pitchPerPx = m_client->editState().scale.pitchPerPx;
  const QList<int> &displayEdoList = Settings::DisplayEdo::get();
  int displayEdo = displayEdoList.size();
  const Scale &scale = m_client->editState().scale;
  bool octave_display_a = Settings::OctaveDisplayA::get();

  // This structure is needed since we use the data here multiple times (first
  // for the background, then for the foreground left piano). In the past the
  // left piano was just drawn on a separate pixmap, but that was slow for high
  // resolutions.
  std::vector<BackgroundKeyRow> background_key_rows;
  int background_key_floor_h = PITCH_PER_OCTAVE / pitchPerPx / displayEdo;
  for (int row = 0; true; ++row) {
    // Start the backgrounds on an A just so that the key pattern lines up
    int a_above_max_key =
        (EVENTMAX_KEY / PITCH_PER_OCTAVE + 1) * PITCH_PER_OCTAVE;
    int pitch = quantize_pitch(
        a_above_max_key - row * PITCH_PER_OCTAVE / displayEdo, displayEdo);
    int nextPitch =
        quantize_pitch(pitch - PITCH_PER_OCTAVE / displayEdo, displayEdo);
    BackgroundKeyRow r;
    if (pitch == EVENTDEFAULT_KEY) {
      r.color = BackgroundKeyRow::Root;
    } else {
      if (displayEdoList[nonnegative_modulo(-row, displayEdo)]) {
        r.color = BackgroundKeyRow::Black;
      } else {
        r.color = BackgroundKeyRow::White;
      }
    }

    int this_y = scale.pitchToY(pitch);
    int next_y = scale.pitchToY(nextPitch);

    if (this_y > size().height()) break;
    // Because of rounding error, calculate height by subbing next from this
    int h = next_y - this_y - 1;
    r.y = this_y - background_key_floor_h / 2;
    r.h = h;
    r.pitch = pitch;
    background_key_rows.push_back(r);
  }

  for (const BackgroundKeyRow &r : background_key_rows) {
    QBrush *brush;
    switch (r.color) {
      case BackgroundKeyRow::Root:
        brush = &rootNoteBrush;
        break;
      case BackgroundKeyRow::White:
        brush = &whiteNoteBrush;
        break;
      case BackgroundKeyRow::Black:
        brush = &blackNoteBrush;
        break;
    }
    if (m_dark) brush = &black;
    painter.fillRect(0, r.y, size().width(), r.h, *brush);
  }

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

  int clock = m_client->controller()->m_audio_renderer->moo_timing().now_clock;

  // Draw the note blocks! Upon hitting an event, see if we are able to draw a
  // previous block.
  std::optional<Interval> selection = std::nullopt;
  std::set<int> selected_unit_nos = m_client->selectedUnitNos();
  if (m_dark)
    painter.setCompositionMode(QPainter::CompositionMode_Screen);
  else
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

  if (m_client->editState().mouse_edit_state.selection.has_value() &&
      m_client->clipboard()->kindIsCopied(EVENTKIND_VELOCITY))
    selection = m_client->editState().mouse_edit_state.selection.value();

  constexpr double DISTANCE_THRESHOLD_SQ = 12 * 12;
  double min_segment_distance_to_mouse = DISTANCE_THRESHOLD_SQ;
  int min_segment_unit_no = -1;

  std::vector<LeftPianoNote> left_piano_notes;
  std::vector<UnitDrawParam> unit_draw_params;
  UnitDrawParam current_unit_draw_param, hover_unit_draw_param;
  std::vector<NoteSegment> current_unit_notes, hover_unit_notes;
  for (int unit_no = 0; unit_no < m_pxtn->Unit_Num(); ++unit_no) {
    qint32 unit_id = m_client->unitIdMap().noToId(unit_no);
    std::shared_ptr<const NoteBrush> brush = StyleEditor::noteBrush(unit_id);
    bool matchingUnit = (unit_id == m_client->editState().m_current_unit_id);
    bool hoveredUnit =
        unit_no == m_focused_unit_no || unit_no == m_hovered_unit_no;
    bool selected = selected_unit_nos.find(unit_no) != selected_unit_nos.end();
    bool visible = m_pxtn->Unit_Get(unit_no)->get_visible();
    int alpha;
    if (hoveredUnit) {
      if (matchingUnit)
        alpha = 255;
      else
        alpha = 192;
    } else {
      if (matchingUnit)
        alpha = 255;
      else if (visible)
        alpha = (m_dark ? 216 : 96);
      else
        alpha = 0;
      if (m_focused_unit_no.has_value() || m_hovered_unit_no.has_value())
        alpha /= 2;
    }
    bool muted = !m_pxtn->Unit_Get(unit_no)->get_played();
    bool draw_left_highlight = matchingUnit || hoveredUnit || m_dark;
    UnitDrawParam param{brush,   matchingUnit, hoveredUnit, draw_left_highlight,
                        visible, alpha,        m_dark,      muted,
                        selected};
    unit_draw_params.push_back(param);
    if (matchingUnit) current_unit_draw_param = param;
    if (hoveredUnit) hover_unit_draw_param = param;
  }

  double seconds_per_clock = 60 / m_client->pxtn()->master->get_beat_tempo() /
                             m_client->pxtn()->master->get_beat_clock();
  double decay_length = 0.25 / seconds_per_clock;
  auto handleNoteSegment = [&](int unit_no, const DrawState &state, int end) {
    Interval segment{state.pitch.clock, end};
    const MouseEditState &mouse = m_client->editState().mouse_edit_state;
    const UnitDrawParam &param = unit_draw_params[unit_no];
    Interval on = state.ongoingOnEvent.value();
    Interval interval = interval_intersect(on, segment);

    // Determine distance to mouse, for mouse seek
    if (m_select_unit_enabled &&
        std::holds_alternative<MouseKeyboardEdit>(mouse.kind) &&
        param.visible) {
      double d = distance_to_mouse(mouse, interval, state.pitch.value,
                                   displayEdo, scale);
      if (d < min_segment_distance_to_mouse) {
        min_segment_unit_no = unit_no;
        min_segment_distance_to_mouse = d;
      }
    }

    double on_strength = 0;
    if (m_client->isPlaying() || true) {
      if (clock >= on.start) {
        if (!m_dark)
          on_strength = clock < on.end ? 1 : 0;
        else {
          if (clock < on.end) {
            double seconds_in_block = (clock - on.start) * seconds_per_clock;
            on_strength = lerp_f(seconds_in_block / 0.25, 1, 0.6);
          } else if (clock < on.end + decay_length) {
            on_strength = lerp_f((clock - on.end) / decay_length, 0.5, 0.0);
          }
        }
      }
    }
    int pitch = state.pitch.value;
    int velocity = state.velocity.value;
    NoteSegment note{interval, on_strength, pitch, velocity, on};

    // draw background highlight & record left piano note
    if (note.on_strength > 0 && interval.start == on.start && !param.muted &&
        param.alpha > 0) {
      int alpha;
      double velocity_strength = double(note.velocity) / EVENTMAX_VELOCITY;
      if (!m_dark)
        alpha = 32 * velocity_strength * (param.alpha / 2 + 128) / 256;
      else
        alpha = lerp(velocity_strength * param.alpha / 256, 32, 72);
      QColor c = param.brush->toQColor(128, 0, alpha * note.on_strength);
      paintBlock(state.pitch.value,
                 Interval{0, int(scale.clockPerPx * width())}, painter, c,
                 scale, displayEdo);
      c.setAlpha((128 + c.alpha() / 2) * note.on_strength);
      left_piano_notes.push_back({state.pitch.value, c});
    }

    if (interval_intersect(interval, clockBounds).empty()) return;
    if (param.matchingUnit)
      current_unit_notes.push_back(note);
    else if (param.hoveredUnit)
      hover_unit_notes.push_back(note);
    else
      drawNoteSegment(painter, param, note, scale, displayEdo, selection);
  };

  for (const EVERECORD *e = m_pxtn->evels->get_Records(); e != nullptr;
       e = e->next) {
    // if (e->clock > clockBounds.end) break;
    int unit_no = e->unit_no;
    DrawState &state = drawStates[unit_no];
    switch (e->kind) {
      case EVENTKIND_ON:
        // Draw the last block of the previous on event if there's one to
        // draw.
        if (state.ongoingOnEvent.has_value())
          handleNoteSegment(unit_no, state, e->clock);
        state.ongoingOnEvent.emplace(Interval{e->clock, e->value + e->clock});
        break;
      case EVENTKIND_VELOCITY:
        state.velocity.set(e);
        break;
      case EVENTKIND_KEY:
        // Maybe draw the previous segment of the current on event.
        if (state.ongoingOnEvent.has_value()) {
          handleNoteSegment(unit_no, state, e->clock);
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
      DrawState &state = drawStates[unit_no];
      handleNoteSegment(unit_no, state, state.ongoingOnEvent.value().end);
      state.ongoingOnEvent.reset();
    }
  }

  // Draw the active and hover units
  const auto &mouse = m_client->editState().mouse_edit_state;
  for (const NoteSegment &note : current_unit_notes) {
    drawNoteSegment(painter, current_unit_draw_param, note, scale, displayEdo,
                    selection);
    maybeDrawNoteVelTooltip(painter, note, current_unit_draw_param, mouse,
                            scale, displayEdo, selection);
  }
  for (const NoteSegment &note : hover_unit_notes)
    drawNoteSegment(painter, hover_unit_draw_param, note, scale, displayEdo,
                    selection);

  if (min_segment_distance_to_mouse < DISTANCE_THRESHOLD_SQ &&
      min_segment_unit_no >= 0)
    setHoveredUnitNo(min_segment_unit_no);
  else
    setHoveredUnitNo(std::nullopt);

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
                        selectionAlphaMultiplier, displayEdo);
    }
  }
  double mySelectionAlphaMultiplier =
      m_client->clipboard()->kindIsCopied(EVENTKIND_VELOCITY) ? 1 : 0.7;
  drawExistingSelection(painter, m_client->editState().mouse_edit_state,
                        m_client->editState().scale.clockPerPx, size().height(),
                        mySelectionAlphaMultiplier);
  if (!m_select_unit_enabled)
    drawOngoingAction(
        m_client->editState(), m_edit_state, painter, width(), height(),
        m_client->controller()->m_audio_renderer->moo_timing().now_no_wrap(
            m_pxtn->master),
        m_pxtn->master, 1, 1, displayEdo);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

  drawLastSeek(painter, m_client, height(), false);
  drawRepeatAndEndBars(painter, m_pxtn->master,
                       m_client->editState().scale.clockPerPx, height());

  // Left piano
  {
    painter.setOpacity(m_dark ? 0.7 : 1);
    QTransform t = painter.transform();
    // we use event_rect_f in case e->rect has some rounding from scaling up
    painter.setTransform(QTransform::fromTranslate(event_rect_f.left(), 0),
                         true);
    QBrush *leftBrush, *leftInnerBrush;
    for (const BackgroundKeyRow &r : background_key_rows) {
      switch (r.color) {
        case BackgroundKeyRow::Root:
        case BackgroundKeyRow::White:
          leftBrush = &whiteLeftBrush;
          leftInnerBrush = nullptr;
          break;
        case BackgroundKeyRow::Black:
          leftBrush = &blackLeftBrush;
          leftInnerBrush = &blackLeftInnerBrush;
          break;
      }
      painter.fillRect(0, r.y - 1, Settings::LeftPianoWidth::get(), 1,
                       blackLeftInnerBrush);
      drawLeftPiano(painter, r.y, r.h, *leftBrush, leftInnerBrush);
      int pitch_offset = 0;
      if (!octave_display_a) pitch_offset = PITCH_PER_OCTAVE / 4;
      if (nonnegative_modulo(r.pitch - EVENTDEFAULT_KEY, PITCH_PER_OCTAVE) ==
          pitch_offset)
        drawOctaveNumAlignBottomLeft(
            &painter, 4, r.y + r.h - 2,
            (r.pitch - PITCH_PER_OCTAVE / 4) / PITCH_PER_OCTAVE - 3,
            background_key_floor_h, octave_display_a);
    }

    // Highlights from song notes
    for (const LeftPianoNote &n : left_piano_notes)
      drawLeftPianoNoteHighlight(painter, n.pitch, scale, displayEdo, n.c);

    // Highlights from MIDI pitches
    for (auto &[pitch, vel] : m_midi_notes) {
      const NoteBrush &brush =
          *StyleEditor::noteBrush(m_client->editState().m_current_unit_id);
      drawLeftPianoNoteHighlight(painter, pitch, m_client->editState().scale,
                                 displayEdo, brush.toQColor(vel, 1, 200));
    }

    // Highlight from clicked note
    std::optional<std::pair<int, int>> left_piano_pitch_and_vel;
    if (auto *keyboard_edit_state = std::get_if<MouseKeyboardEdit>(
            &m_client->editState().mouse_edit_state.kind))
      if (auto *s =
              std::get_if<MouseLeftKeyboard>(&keyboard_edit_state->kind)) {
        left_piano_pitch_and_vel = {keyboard_edit_state->current_pitch,
                                    s->start_vel};
      }
    if (left_piano_pitch_and_vel.has_value() && m_audio_note_preview) {
      int pitch = quantize_pitch(left_piano_pitch_and_vel.value().first,
                                 m_edit_state.m_quantize_pitch);
      int vel = left_piano_pitch_and_vel.value().second;
      drawLeftPianoNoteHighlight(
          painter, pitch, scale, displayEdo,
          QColor::fromRgb(255, 255, 255, 32 + 64 * vel / EVENTMAX_VELOCITY));
    }
    painter.setOpacity(1);
    painter.setTransform(t);
  }

  drawCurrentPlayerPosition(painter, clock, height(),
                            m_client->editState().scale.clockPerPx, false);

  // Draw cursors
  for (const auto &[uid, remote_state] : m_client->remoteEditStates()) {
    if (uid == m_client->following_uid() || uid == m_client->uid()) continue;
    if (remote_state.state.has_value()) {
      EditState state = remote_state.state.value();
      state.scale =
          m_client->editState().scale;  // Position according to our scale
      int unit_id = state.m_current_unit_id;
      // Draw cursor
      QColor color;
      if (unit_id != m_client->editState().m_current_unit_id)
        color = StyleEditor::noteBrush(unit_id)->toQColor(EVENTMAX_VELOCITY, 0,
                                                          128);
      else
        color = StyleEditor::config.color.Cursor;
      drawCursor(state, painter, color, remote_state.user, uid);
    }
  }
  {
    QString my_username = "";
    auto it = m_client->remoteEditStates().find(m_client->following_uid());
    if (it != m_client->remoteEditStates().end()) my_username = it->second.user;
    drawCursor(m_client->editState(), painter, StyleEditor::config.color.Cursor,
               my_username, m_client->following_uid());
  }

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

void KeyboardView::moveEvent(QMoveEvent *e) {
  update();
  QWidget::moveEvent(e);
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
                impliedVelocity(e.mouse_edit_state, e.scale.pitchPerPx));
        },
        false);
    event->accept();
  }
}

void KeyboardView::updateStatePositions(EditState &edit_state,
                                        const QMouseEvent *event) {
  MouseEditState &state = edit_state.mouse_edit_state;
  QPointF mouse_pos = worldTransform().inverted().map(event->localPos());
  // The logical coordinates of the top-left corner of the viewport.
  QPointF viewport_pos = worldTransform().inverted().map(QPointF(-pos()));
  state.current_clock =
      std::max(0., mouse_pos.x() * edit_state.scale.clockPerPx);
  int current_pitch = edit_state.scale.pitchOfY(mouse_pos.y());
  int left_keyboard_current_vel =
      int(EVENTMAX_VELOCITY * (mouse_pos.x() - viewport_pos.x()) /
          Settings::LeftPianoWidth::get());
  std::variant<MouseLeftKeyboard, MouseMainKeyboard> kind;

  if (mouse_pos.x() < viewport_pos.x() + Settings::LeftPianoWidth::get() &&
      state.type == MouseEditState::Type::Nothing)
    kind = MouseLeftKeyboard{left_keyboard_current_vel};
  else
    kind = MouseMainKeyboard{current_pitch};

  if (state.type == MouseEditState::Type::Nothing ||
      state.type == MouseEditState::Type::Seek ||
      !std::holds_alternative<MouseKeyboardEdit>(state.kind)) {
    state.type = (event->modifiers() & Qt::ShiftModifier
                      ? MouseEditState::Type::Seek
                      : MouseEditState::Type::Nothing);
    state.start_clock = state.current_clock;
    state.kind.emplace<MouseKeyboardEdit>(
        MouseKeyboardEdit{kind, current_pitch});
  }
  auto &keyboard_edit_state = std::get<MouseKeyboardEdit>(state.kind);
  keyboard_edit_state.current_pitch = current_pitch;
  emit setScrollOnClick(
      std::holds_alternative<MouseMainKeyboard>(keyboard_edit_state.kind));
}

void KeyboardView::mousePressEvent(QMouseEvent *event) {
  if (m_select_unit_enabled && event->button() & Qt::LeftButton &&
      m_hovered_unit_no.has_value()) {
    m_client->changeEditState(
        [&](EditState &s) {
          s.m_current_unit_id =
              m_client->unitIdMap().noToId(m_hovered_unit_no.value());
        },
        false);
    return;
  }

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
            const auto &mouse_keyboard_state =
                std::get<MouseKeyboardEdit>(s.mouse_edit_state.kind);
            int pitch = mouse_keyboard_state.current_pitch;
            pitch = quantize_pitch(pitch, m_edit_state.m_quantize_pitch);
            s.mouse_edit_state.last_pitch = pitch;

            int clock = m_client->editState().mouse_edit_state.current_clock;
            clock = quantize(clock, m_edit_state.m_quantize_clock);

            qint32 unit_no = maybe_unit_no.value();
            qint32 vel =
                m_pxtn->evels->get_Value(clock, unit_no, EVENTKIND_VELOCITY);
            s.mouse_edit_state.base_velocity = vel;
            bool chord_preview;
            if (const auto *left_kb_state = std::get_if<MouseLeftKeyboard>(
                    &mouse_keyboard_state.kind)) {
              vel = left_kb_state->start_vel;
              chord_preview = false;
            } else
              chord_preview =
                  Settings::ChordPreview::get() && !m_client->isPlaying();

            m_audio_note_preview = std::make_unique<NotePreview>(
                m_pxtn, &m_client->moo()->params, unit_no, clock, pitch, vel,
                m_client->audioState()->bufferSize(), chord_preview, this);
          }
        }
      },
      true);
}

void KeyboardView::mouseMoveEvent(QMouseEvent *event) {
  if (!m_client->isFollowing()) {
    m_client->changeEditState([&](auto &s) { updateStatePositions(s, event); },
                              true);
  }
  if (m_audio_note_preview != nullptr) {
    bool is_left_kb = false;
    const auto *keyboard_edit_state = std::get_if<MouseKeyboardEdit>(
        &m_client->editState().mouse_edit_state.kind);
    if (const auto *keyboard_edit_state = std::get_if<MouseKeyboardEdit>(
            &m_client->editState().mouse_edit_state.kind))
      is_left_kb =
          std::holds_alternative<MouseLeftKeyboard>(keyboard_edit_state->kind);
    if (is_left_kb) {
      int pitch = quantize_pitch(keyboard_edit_state->current_pitch,
                                 m_edit_state.m_quantize_pitch);
      if (pitch != m_last_left_kb_pitch) {
        m_audio_note_preview->processEvent(EVENTKIND_KEY, pitch);
        m_audio_note_preview->processEvent(EVENTKIND_PORTAMENT, 0);
        m_audio_note_preview->resetOn(10000000);
        m_last_left_kb_pitch = pitch;
      }
    } else {
      m_audio_note_preview->processEvent(
          EVENTKIND_VELOCITY,
          impliedVelocity(m_client->editState().mouse_edit_state,
                          m_client->editState().scale.pitchPerPx));
    }
  }
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

void KeyboardView::cycleCurrentUnit(int offset, bool selectedOnly) {
  if (!canChangeUnit(m_client)) return;

  auto maybe_unit_no =
      m_client->unitIdMap().idToNo(m_client->editState().m_current_unit_id);
  if (maybe_unit_no == std::nullopt) return;

  qint32 starting_unit_no = maybe_unit_no.value();
  qint32 unit_no =
      nonnegative_modulo(starting_unit_no + offset, m_pxtn->Unit_Num());
  int sign = (offset >= 0 ? 1 : -1);

  for (int i = 0; i < m_pxtn->Unit_Num(); ++i) {
    if (!selectedOnly || m_client->pxtn()->Unit_Get(unit_no)->get_operated() ||
        unit_no == starting_unit_no)
      break;
    unit_no = nonnegative_modulo(unit_no + sign, m_pxtn->Unit_Num());
  }

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
  const auto &e = m_client->editState();
  if (e.mouse_edit_state.selection.has_value()) {
    int offset;
    switch (dir) {
      case Direction::UP:
        offset = 1;
        break;
      case Direction::DOWN:
      default:
        offset = -1;
        break;
    }
    EVENTKIND kind;
    if (shift) {
      kind = EVENTKIND_KEY;
      offset *= wide ? PITCH_PER_OCTAVE
                     : int(double(PITCH_PER_OCTAVE) /
                               std::min(e.m_quantize_pitch_denom, 36) +
                           0.5);
    } else {
      EVENTKIND current_kind =
          paramOptions()[e.m_current_param_kind_idx].second;
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
    Interval interval(e.mouse_edit_state.selection.value());

    using namespace Action;
    std::list<Primitive> as;
    for (qint32 unitNo : m_client->selectedUnitNos()) {
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

  // the optional int is a proxy for if we're actually modifying the main
  // keyboard vs. previewing in the left kbd
  std::optional<int> start_pitch;
  if (const auto *mouse_main_keyboard =
          std::get_if<MouseMainKeyboard>(&keyboard_edit_state.kind)) {
    start_pitch = quantize_pitch(mouse_main_keyboard->start_pitch,
                                 m_edit_state.m_quantize_pitch);
  }
  // int end_pitch = int(round(pitchOfY(event->localPos().y())));

  m_client->changeEditState(
      [&](auto &s) {
        if (m_pxtn->Unit_Num() > 0) {
          using namespace Action;
          std::list<Primitive> actions;
          switch (s.mouse_edit_state.type) {
            case MouseEditState::SetOn:
            case MouseEditState::DeleteOn:
              if (start_pitch.has_value()) {
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
                                      m_client->editState().scale.pitchPerPx);
                  s.mouse_edit_state.base_velocity = vel;
                  actions.push_back({EVENTKIND_VELOCITY,
                                     m_client->editState().m_current_unit_id,
                                     clock_int.start, Add{vel}});
                  actions.push_back(
                      {EVENTKIND_KEY, m_client->editState().m_current_unit_id,
                       clock_int.start, Add{start_pitch.value()}});
                }
              }
              break;
            case MouseEditState::SetNote:
            case MouseEditState::DeleteNote:
              if (start_pitch.has_value()) {
                actions.push_back({EVENTKIND_KEY,
                                   m_client->editState().m_current_unit_id,
                                   clock_int.start, Delete{clock_int.end}});

                if (m_client->editState().mouse_edit_state.type ==
                    MouseEditState::SetNote) {
                  int current_pitch =
                      quantize_pitch(keyboard_edit_state.current_pitch,
                                     m_edit_state.m_quantize_pitch);
                  const auto intervals =
                      vibratoIntervals(clock_int, m_client->quantizeClock(),
                                       start_pitch.value(), current_pitch);
                  for (const auto &[interval, pitch] : intervals) {
                    actions.push_back({EVENTKIND_KEY,
                                       m_client->editState().m_current_unit_id,
                                       interval.start, Add{pitch}});
                  }
                }
              }
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

void KeyboardView::copySelection() {
  if (!m_client->editState().mouse_edit_state.selection.has_value()) return;
  m_client->clipboard()->copy(
      m_client->selectedUnitNos(),
      m_client->editState().mouse_edit_state.selection.value(), m_pxtn,
      m_client->controller()->woiceIdMap());
}

void KeyboardView::clearSelection() {
  if (!m_client->editState().mouse_edit_state.selection.has_value()) return;
  std::list<Action::Primitive> actions = m_client->clipboard()->makeClear(
      m_client->selectedUnitNos(),
      m_client->editState().mouse_edit_state.selection.value(),
      m_client->unitIdMap());
  if (actions.size() > 0) m_client->applyAction(actions);
}

void KeyboardView::cutSelection() {
  copySelection();
  clearSelection();
}

void KeyboardView::quantizeSelectionX() {
  const static std::set<EVENTKIND> kindsToQuantize(
      {EVENTKIND_VELOCITY, EVENTKIND_KEY, EVENTKIND_ON});
  if (!m_client->editState().mouse_edit_state.selection.has_value()) return;
  const Interval &range =
      m_client->editState().mouse_edit_state.selection.value();
  const std::set<int> &unit_nos(m_client->selectedUnitNos());
  LocalEditState localState(m_pxtn, m_client->editState());
  int quantizeClock = localState.m_quantize_clock;
  // Quantize with rounding, rather than flooring / ceiling is more useful
  // for MIDI input.
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
    qint32 start_clock = quantize(e->clock);
    if (start_clock >= range.end) continue;
    if (unit_nos.find(e->unit_no) != unit_nos.end() &&
        kindsToQuantize.find(kind) != kindsToQuantize.end()) {
      int unit_id = m_client->unitIdMap().noToId(e->unit_no);
      if (Evelist_Kind_IsTail(e->kind)) {
        // We round the end time up if it's too small. Even though this
        // might cause an add overlap with the next value, this should be
        // okay in terms of interacting with undo, since the undos of both
        // of these is 2 clears. (It takes some effort to explain)

        // 2021-10-13: However we don't let it go beyond the selection
        // end.
        int v = std::min(std::max(quantizeClock, quantize(e->value)),
                         range.end - start_clock);
        actions.push_back({kind, unit_id, start_clock, Action::Add{v}});
      } else
        actions.push_back({kind, unit_id, start_clock, Action::Add{e->value}});
    }
  }
  qDebug() << "apply quantizeX";
  m_client->applyAction(actions);
}

void KeyboardView::quantizeSelectionY() {
  if (!m_client->editState().mouse_edit_state.selection.has_value()) return;
  const Interval &range =
      m_client->editState().mouse_edit_state.selection.value();
  const std::set<int> &unit_nos(m_client->selectedUnitNos());
  int denom = m_client->editState().m_quantize_pitch_denom;

  const EVERECORD *e = nullptr;
  for (e = m_pxtn->evels->get_Records(); e; e = e->next)
    if (e->clock >= range.start) break;

  std::list<Action::Primitive> actions;
  for (int unit_no : unit_nos) {
    int unit_id = m_client->unitIdMap().noToId(unit_no);
    actions.push_back(
        {EVENTKIND_KEY, unit_id, range.start, Action::Delete{range.end}});
  }

  for (; e && e->clock < range.end; e = e->next) {
    if (e->kind != EVENTKIND_KEY) continue;
    if (unit_nos.find(e->unit_no) == unit_nos.end()) continue;
    int unit_id = m_client->unitIdMap().noToId(e->unit_no);
    actions.push_back({EVENTKIND_KEY, unit_id, e->clock,
                       Action::Add{quantize_pitch(e->value, denom)}});
  }
  qDebug() << "apply quantizeY";
  m_client->applyAction(actions);
}

void KeyboardView::paste(bool useSelectionEnd, bool preserveFollow) {
  if (!m_client->editState().mouse_edit_state.selection.has_value()) return;
  m_client->changeEditState(
      [&](auto &s) {
        Interval &selection = s.mouse_edit_state.selection.value();
        qint32 paste_point = useSelectionEnd ? selection.end : selection.start;
        PasteResult paste_result = m_client->clipboard()->makePaste(
            m_client->selectedUnitNos(), paste_point, m_client->unitIdMap());
        if (paste_result.actions.size() > 0)
          m_client->applyAction(paste_result.actions);
        selection.start = paste_point;
        selection.end = selection.start + paste_result.length;
      },
      preserveFollow);
}

void KeyboardView::toggleDark() { m_dark = !m_dark; }
