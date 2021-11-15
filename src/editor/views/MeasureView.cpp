#include "MeasureView.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>

#include "ViewHelper.h"
#include "editor/ComboOptions.h"
#include "editor/Settings.h"
#include "editor/StyleEditor.h"

MeasureView::MeasureView(PxtoneClient *client, MooClock *moo_clock,
                         QWidget *parent)
    : QWidget(parent),
      m_client(client),
      m_anim(new Animation(this)),
      m_moo_clock(moo_clock),
      m_audio_note_preview(nullptr) {
  setFocusPolicy(Qt::NoFocus);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
  updateGeometry();
  setMouseTracking(true);
  connect(m_anim, &Animation::nextFrame, [this]() { update(); });
  connect(m_client, &PxtoneClient::editStateChanged,
          [this](const EditState &s) {
            if (!(m_last_scale == s.scale)) updateGeometry();
            m_last_scale = s.scale;
          });
  connect(m_client->controller(), &PxtoneController::measureNumChanged, this,
          &QWidget::updateGeometry);
}

void drawCursor(const EditState &state, QPainter &painter, const QColor &color,
                const QString &username, qint64 uid) {
  if (!std::holds_alternative<MouseMeasureEdit>(state.mouse_edit_state.kind))
    return;
  int y = std::get<MouseMeasureEdit>(state.mouse_edit_state.kind).y;
  QPoint position(state.mouse_edit_state.current_clock / state.scale.clockPerPx,
                  y);
  drawCursor(position, painter, color, username, uid);
}

enum struct FlagType : qint8 { Top, Repeat, Last };
constexpr int FLAG_HEIGHT = 8;
constexpr int FLAG_WIDTH = 40;
static void drawFlag(QPainter *painter, FlagType type, bool outline,
                     int measure_x, int y) {
  int sx, sy;
  bool end;
  switch (type) {
    case FlagType::Top:
      sx = 0;
      sy = 0;
      end = false;
      break;
    case FlagType::Repeat:
      sx = 0;
      sy = 8;
      end = false;
      break;
    case FlagType::Last:
      sx = FLAG_WIDTH;
      sy = 8;
      end = true;
      break;
  }
  if (outline) sy += 8;
  int x = (end ? measure_x - FLAG_WIDTH : measure_x + 1);
  painter->drawPixmap(x, y, *StyleEditor::measureImages(), sx, sy, FLAG_WIDTH,
                      FLAG_HEIGHT);
}

constexpr int MEASURE_NUM_BLOCK_WIDTH = 27;
constexpr int MEASURE_NUM_BLOCK_HEIGHT = 10;
constexpr int RULER_HEIGHT = 15;
constexpr int SEPARATOR_OFFSET = 6;
constexpr int FLAG_Y = MEASURE_NUM_BLOCK_HEIGHT;
constexpr int UNIT_EDIT_HEIGHT = 15;
constexpr int UNIT_EDIT_OFFSET = 2;
constexpr int RIBBON_HEIGHT =
    MEASURE_NUM_BLOCK_HEIGHT + RULER_HEIGHT + SEPARATOR_OFFSET;
constexpr int UNIT_EDIT_Y = RIBBON_HEIGHT + UNIT_EDIT_OFFSET;
void drawOngoingAction(const EditState &state, QPainter &painter, int height,
                       int quantizeClock, int clockPerMeas,
                       std::optional<int> nowNoWrap, const pxtnMaster *master,
                       double alphaMultiplier,
                       double selectionAlphaMultiplier) {
  const MouseEditState &mouse_edit_state = state.mouse_edit_state;

  auto drawVelAction = [&](const Interval &interval, qint32 velocity,
                           int alpha) {
    const Brush &brush =
        brushes[nonnegative_modulo(state.m_current_unit_id, NUM_BRUSHES)];
    painter.fillRect(interval.start, UNIT_EDIT_Y + 3, interval.length(),
                     UNIT_EDIT_HEIGHT - 6,
                     brush.toQColor(velocity, false, alpha * alphaMultiplier));
  };

  {
    Interval interval(mouse_edit_state.clock_int(quantizeClock) /
                      state.scale.clockPerPx);
    qint32 velocity = qint32(round(mouse_edit_state.base_velocity));
    switch (mouse_edit_state.type) {
      case MouseEditState::Type::SetOn:
      case MouseEditState::Type::DeleteOn:
      case MouseEditState::Type::SetNote:
      case MouseEditState::Type::DeleteNote:
        if (std::holds_alternative<MouseMeasureEdit>(
                state.mouse_edit_state.kind))
          drawVelAction(interval, velocity, 255);
        break;
      case MouseEditState::Type::Nothing: {
        if (std::holds_alternative<MouseMeasureEdit>(
                state.mouse_edit_state.kind)) {
          if (std::get<MouseMeasureEdit>(state.mouse_edit_state.kind).y <
              RIBBON_HEIGHT) {
            int half_meas =
                2 * mouse_edit_state.current_clock / clockPerMeas + 1;
            int meas = half_meas / 2;
            int left_half = half_meas % 2 == 1;
            drawFlag(&painter, (left_half ? FlagType::Repeat : FlagType::Last),
                     true, meas * clockPerMeas / state.scale.clockPerPx,
                     FLAG_Y);
          } else
            drawVelAction(interval, velocity, 96);
        }
        break;
      }
      case MouseEditState::Type::Seek: {
        QColor color = StyleEditor::palette.Playhead;
        color.setAlpha(color.alpha() * alphaMultiplier / 2);
        drawPlayhead(painter,
                     mouse_edit_state.current_clock / state.scale.clockPerPx,
                     height, color, true);
        break;
      }
      case MouseEditState::Type::Select: {
        drawSelection(painter, interval, height, selectionAlphaMultiplier);
      } break;
    }
  }

  if (state.m_input_state.has_value() && nowNoWrap.has_value()) {
    const Input::State::On &v = state.m_input_state.value();

    for (const Interval &interval : v.clock_ints(nowNoWrap.value(), master))
      drawVelAction(interval / state.scale.clockPerPx, v.on.vel, 255);
  }
}

QSize MeasureView::sizeHint() const {
  return QSize(one_over_last_clock(m_client->pxtn()) /
                   m_client->editState().scale.clockPerPx,
               1 + RIBBON_HEIGHT + UNIT_EDIT_OFFSET + UNIT_EDIT_HEIGHT);
}

void MeasureView::paintEvent(QPaintEvent *e) {
  const pxtnService *pxtn = m_client->pxtn();

  QPainter painter(this);
  painter.fillRect(0, 0, width(), height(), Qt::black);

  // Draw white lines under background
  // TODO: Dedup with keyboardview
  const pxtnMaster *master = pxtn->master;
  int clockPerMeas = master->get_beat_num() * master->get_beat_clock();
  int activeMeas = std::max(master->get_last_meas(), master->get_meas_num());
  qreal activeWidth =
      activeMeas * clockPerMeas / m_client->editState().scale.clockPerPx;
  int lastMeasureDraw = -MEASURE_NUM_BLOCK_WIDTH - 1;
  painter.fillRect(0, MEASURE_NUM_BLOCK_HEIGHT, activeWidth, RULER_HEIGHT,
                   StyleEditor::palette.MeasureIncluded);
  painter.fillRect(activeWidth, MEASURE_NUM_BLOCK_HEIGHT, width() - activeWidth,
                   RULER_HEIGHT, StyleEditor::palette.MeasureExcluded);
  painter.fillRect(0,
                   MEASURE_NUM_BLOCK_HEIGHT + RULER_HEIGHT + SEPARATOR_OFFSET,
                   width(), 1, StyleEditor::palette.MeasureBeat);
  for (int beat = 0; true; ++beat) {
    int x = beat * master->get_beat_clock() /
            m_client->editState().scale.clockPerPx;
    if (x > width()) break;
    if (beat % master->get_beat_num() == 0) {
      int measure = beat / master->get_beat_num();
      painter.fillRect(x, MEASURE_NUM_BLOCK_HEIGHT, 1, size().height(),
                       StyleEditor::palette.MeasureSeparator);
      if (x - lastMeasureDraw < MEASURE_NUM_BLOCK_WIDTH) continue;
      lastMeasureDraw = x;
      painter.fillRect(x, 0, 1, MEASURE_NUM_BLOCK_HEIGHT,
                       StyleEditor::palette.MeasureSeparator);
      painter.fillRect(x + 1, 0, MEASURE_NUM_BLOCK_WIDTH,
                       MEASURE_NUM_BLOCK_HEIGHT,
                       StyleEditor::palette.MeasureNumberBlock);
      if (measure < activeMeas)
        drawNumAlignTopRight(&painter, x + MEASURE_NUM_BLOCK_WIDTH, 1,
                             beat / master->get_beat_num());
    } else
      painter.fillRect(x, MEASURE_NUM_BLOCK_HEIGHT + RULER_HEIGHT, 1, height(),
                       StyleEditor::palette.MeasureBeat);
  }
  drawFlag(&painter, FlagType::Top, false, 0, FLAG_Y);
  if (m_moo_clock->repeat_clock() > 0) {
    drawFlag(
        &painter, FlagType::Repeat, false,
        m_moo_clock->repeat_clock() / m_client->editState().scale.clockPerPx,
        FLAG_Y);
  }
  if (m_moo_clock->has_last()) {
    drawFlag(&painter, FlagType::Last, false,
             m_moo_clock->last_clock() / m_client->editState().scale.clockPerPx,
             FLAG_Y);
  }

  // Draw on events

  painter.fillRect(0, UNIT_EDIT_Y, width(), UNIT_EDIT_HEIGHT,
                   StyleEditor::palette.MeasureUnitEdit);
  double scaleX = m_client->editState().scale.clockPerPx;
  Interval clockBounds = {
      qint32(e->rect().left() * scaleX) - WINDOW_BOUND_SLACK,
      qint32(e->rect().right() * scaleX) + WINDOW_BOUND_SLACK};
  int unit_id = m_client->editState().m_current_unit_id;
  const Brush &brush = brushes[nonnegative_modulo(unit_id, NUM_BRUSHES)];
  std::optional<int> maybe_unit_no = m_client->unitIdMap().idToNo(unit_id);
  if (maybe_unit_no.has_value()) {
    int unit_no = maybe_unit_no.value();

    std::optional<Interval> last_on = std::nullopt;
    int last_vel = EVENTDEFAULT_VELOCITY;
    int y = UNIT_EDIT_Y + UNIT_EDIT_HEIGHT / 2;

    auto drawLastOn = [&]() {
      if (last_on.has_value()) {
        const Interval &i = last_on.value();
        drawUnitBullet(
            painter, i.start / scaleX, y,
            int(i.end / scaleX) - int(i.start / scaleX),
            brush.toQColor(last_vel, i.contains(m_moo_clock->now()), 255));
      }
      last_on = std::nullopt;
    };

    for (const EVERECORD *e = pxtn->evels->get_Records(); e != nullptr;
         e = e->next) {
      if (e->clock > clockBounds.end) break;
      if (e->unit_no == unit_no) {
        if (e->kind == EVENTKIND_ON) {
          drawLastOn();
          last_on = Interval{e->clock, e->clock + e->value};
        }
        if (e->kind == EVENTKIND_VELOCITY) {
          if (last_on.has_value() && last_on.value().start < e->clock)
            drawLastOn();
          last_vel = e->value;
        }
      }
    }
    drawLastOn();
  }

  drawLastSeek(painter, m_client, height(), true);
  drawCurrentPlayerPosition(painter, m_moo_clock, height(),
                            m_client->editState().scale.clockPerPx, true);
  for (const auto &[uid, remote_state] : m_client->remoteEditStates()) {
    if (uid == m_client->following_uid() || uid == m_client->uid()) continue;
    if (remote_state.state.has_value()) {
      EditState adjusted_state(remote_state.state.value());
      bool same_unit = adjusted_state.m_current_unit_id ==
                       m_client->editState().m_current_unit_id;
      double alphaMultiplier = (same_unit ? 0.7 : 0.3);
      double selectionAlphaMultiplier = (same_unit ? 0.5 : 0.3);
      adjusted_state.scale = m_client->editState().scale;

      drawExistingSelection(painter, adjusted_state.mouse_edit_state,
                            adjusted_state.scale.clockPerPx, height(),
                            selectionAlphaMultiplier);
      drawOngoingAction(
          adjusted_state, painter, height(),
          m_client->quantizeClock(
              quantizeXOptions()[adjusted_state.m_quantize_clock_idx].second),
          clockPerMeas, std::nullopt, m_client->pxtn()->master, alphaMultiplier,
          selectionAlphaMultiplier);
    }
  }
  drawExistingSelection(painter, m_client->editState().mouse_edit_state,
                        m_client->editState().scale.clockPerPx, height(), 1);
  drawOngoingAction(m_client->editState(), painter, height(),
                    m_client->quantizeClock(), clockPerMeas,
                    m_moo_clock->nowNoWrap(), m_client->pxtn()->master, 1, 1);

  // Draw cursors
  for (const auto &[uid, remote_state] : m_client->remoteEditStates()) {
    if (uid == m_client->following_uid() || uid == m_client->uid()) continue;
    if (remote_state.state.has_value()) {
      EditState state = remote_state.state.value();
      if (state.current_param_kind_idx() !=
          m_client->editState().current_param_kind_idx())
        continue;
      state.scale =
          m_client->editState().scale;  // Position according to our scale
      int unit_id = state.m_current_unit_id;
      QColor color;
      if (unit_id != m_client->editState().m_current_unit_id)
        color = brushes[unit_id % NUM_BRUSHES].toQColor(EVENTMAX_VELOCITY,
                                                        false, 128);
      else
        color = StyleEditor::palette.Cursor;
      drawCursor(state, painter, color, remote_state.user, uid);
    }
  }

  {
    QString my_username = "";
    auto it = m_client->remoteEditStates().find(m_client->following_uid());
    if (it != m_client->remoteEditStates().end()) my_username = it->second.user;
    drawCursor(m_client->editState(), painter, StyleEditor::palette.Cursor,
               my_username, m_client->following_uid());
  }
}

static void updateStatePositions(EditState &edit_state,
                                 const QMouseEvent *event) {
  MouseEditState &state = edit_state.mouse_edit_state;

  state.current_clock =
      std::max(0., event->localPos().x() * edit_state.scale.clockPerPx);
  if (state.type == MouseEditState::Nothing ||
      state.type == MouseEditState::Seek) {
    state.start_clock = state.current_clock;
    bool shift = event->modifiers() & Qt::ShiftModifier;
    state.type = (shift ? MouseEditState::Seek : MouseEditState::Nothing);
  }

  state.kind = MouseMeasureEdit{event->y()};
}

void MeasureView::mousePressEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }

  m_client->changeEditState(
      [&](EditState &s) {
        updateStatePositions(s, event);
        MouseEditState::Type &type = s.mouse_edit_state.type;
        if (event->modifiers() & Qt::ShiftModifier) {
          if (event->modifiers() & Qt::ControlModifier &&
              event->button() != Qt::RightButton)
            type = MouseEditState::Type::Select;
          else {
            if (event->button() == Qt::RightButton)
              s.mouse_edit_state.selection.reset();
            type = MouseEditState::Type::Seek;
          }
        } else if (std::get<MouseMeasureEdit>(s.mouse_edit_state.kind).y >
                   RIBBON_HEIGHT) {
          if (event->button() == Qt::LeftButton) {
            type = MouseEditState::Type::SetOn;
            qint32 clock = quantize(s.mouse_edit_state.current_clock,
                                    m_client->quantizeClock());
            std::optional<int> unit_no =
                m_client->unitIdMap().idToNo(s.m_current_unit_id);
            if (unit_no.has_value()) {
              m_audio_note_preview = std::make_unique<NotePreview>(
                  m_client->pxtn(), &m_client->moo()->params, unit_no.value(),
                  clock, std::list<EVERECORD>(),
                  m_client->audioState()->bufferSize(),
                  Settings::ChordPreview::get(), this);
              s.mouse_edit_state.base_velocity =
                  m_client->pxtn()->evels->get_Value(clock, unit_no.value(),
                                                     EVENTKIND_VELOCITY);
            }
          } else
            type = MouseEditState::Type::DeleteOn;
        }
      },
      false);
}

void MeasureView::mouseReleaseEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }
  // if (!std::holds_alternative<MouseMeasureEdit>(
  //        m_client->editState().mouse_edit_state.kind))
  //  return;

  qint32 current_clock = m_client->editState().mouse_edit_state.current_clock;
  m_client->changeEditState(
      [&](EditState &s) {
        if (m_client->pxtn()->Unit_Num() > 0) {
          using namespace Action;
          std::list<Primitive> actions;
          Interval clock_int(m_client->editState().mouse_edit_state.clock_int(
              m_client->quantizeClock()));
          switch (s.mouse_edit_state.type) {
            case MouseEditState::SetOn:
            case MouseEditState::SetNote: {
              std::optional<int> maybe_unit_no =
                  m_client->unitIdMap().idToNo(s.m_current_unit_id);
              if (maybe_unit_no.has_value()) {
                actions.push_back({EVENTKIND_ON, s.m_current_unit_id,
                                   clock_int.start, Delete{clock_int.end}});
                actions.push_back({EVENTKIND_VELOCITY, s.m_current_unit_id,
                                   clock_int.start, Delete{clock_int.end}});
                actions.push_back({EVENTKIND_ON, s.m_current_unit_id,
                                   clock_int.start, Add{clock_int.length()}});
                actions.push_back(
                    {EVENTKIND_VELOCITY, s.m_current_unit_id, clock_int.start,
                     Add{qint32(round(s.mouse_edit_state.base_velocity))}});
              }
            } break;
            case MouseEditState::DeleteOn:
            case MouseEditState::DeleteNote:
              actions.push_back({EVENTKIND_ON, s.m_current_unit_id,
                                 clock_int.start, Delete{clock_int.end}});
              actions.push_back({EVENTKIND_VELOCITY, s.m_current_unit_id,
                                 clock_int.start, Delete{clock_int.end}});
              break;
              // TODO: Dedup w/ the other seek / select responses
            case MouseEditState::Seek:
              if (event->button() & Qt::LeftButton)
                m_client->seekMoo(current_clock);
              break;
            case MouseEditState::Select: {
              Interval clock_int(
                  m_client->editState().mouse_edit_state.clock_int(
                      m_client->quantizeClock()));
              s.mouse_edit_state.selection.emplace(clock_int);
              break;
            }
            case MouseEditState::Nothing:
              if (std::holds_alternative<MouseMeasureEdit>(
                      s.mouse_edit_state.kind)) {
                if (std::get<MouseMeasureEdit>(s.mouse_edit_state.kind).y >=
                    RIBBON_HEIGHT)
                  break;
                int half_meas = 2 * current_clock /
                                    m_client->pxtn()->master->get_beat_clock() /
                                    m_client->pxtn()->master->get_beat_num() +
                                1;
                int meas = half_meas / 2;
                bool left_half = half_meas % 2 == 1;
                if (left_half) {
                  if (m_client->pxtn()->master->get_repeat_meas() == meas)
                    m_client->sendAction(SetRepeatMeas{});
                  else
                    m_client->sendAction(SetRepeatMeas{meas});
                } else {
                  if (m_client->pxtn()->master->get_last_meas() == meas)
                    m_client->sendAction(SetLastMeas{});
                  else
                    m_client->sendAction(SetLastMeas{meas});
                }
                break;
              }
          }
          if (actions.size() > 0) {
            m_client->applyAction(actions);
          }
        }
        m_audio_note_preview.reset();
        s.mouse_edit_state.type = MouseEditState::Type::Nothing;
        updateStatePositions(s, event);
      },
      false);
}

void MeasureView::wheelEvent(QWheelEvent *event) {
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
                qint32(round(e.mouse_edit_state.base_velocity)));
        },
        false);
    event->accept();
  }
}

void MeasureView::mouseMoveEvent(QMouseEvent *event) {
  if (!m_client->isFollowing())
    m_client->changeEditState([&](auto &s) { updateStatePositions(s, event); },
                              true);
  if (m_audio_note_preview != nullptr)
    m_audio_note_preview->processEvent(
        EVENTKIND_VELOCITY,
        qint32(round(m_client->editState().mouse_edit_state.base_velocity)));
  event->ignore();
}
