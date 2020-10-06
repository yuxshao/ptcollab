#include "MeasureView.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>

#include "ViewHelper.h"
#include "editor/ComboOptions.h"

MeasureView::MeasureView(PxtoneClient *client, MooClock *moo_clock,
                         QWidget *parent)
    : QWidget(parent),
      m_client(client),
      m_anim(new Animation(this)),
      m_moo_clock(moo_clock) {
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

constexpr int NUM_WIDTH = 8;
constexpr int NUM_HEIGHT = 8;
constexpr int NUM_OFFSET_X = 0;
constexpr int NUM_OFFSET_Y = 40;
static void drawNum(QPainter *painter, int x, int y, int w, int num) {
  static QPixmap images(":/images/images");
  int xr = x + w;
  do {
    int digit = num % 10;
    xr -= NUM_WIDTH;
    painter->drawPixmap(xr, y, NUM_WIDTH, NUM_HEIGHT, images,
                        NUM_OFFSET_X + digit * NUM_WIDTH, NUM_OFFSET_Y,
                        NUM_WIDTH, NUM_HEIGHT);
    num = (num - digit) / 10;
  } while (num > 0);
}

enum struct FlagType { Top, Repeat, Last };
constexpr int FLAG_HEIGHT = 8;
constexpr int FLAG_WIDTH = 40;
static void drawFlag(QPainter *painter, FlagType type, bool outline,
                     int measure_x, int y) {
  static QPixmap images(":/images/images");
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
  painter->drawPixmap(x, y, images, sx, sy, FLAG_WIDTH, FLAG_HEIGHT);
}

constexpr int MEASURE_NUM_BLOCK_WIDTH = 27;
constexpr int MEASURE_NUM_BLOCK_HEIGHT = 10;
constexpr int RULER_HEIGHT = 15;
constexpr int SEPARATOR_OFFSET = 6;
constexpr int FLAG_Y = MEASURE_NUM_BLOCK_HEIGHT;
void drawOngoingAction(const EditState &state, QPainter &painter, int height,
                       int quantizeClock, int clockPerMeas,
                       double alphaMultiplier,
                       double selectionAlphaMultiplier) {
  const MouseEditState &mouse_edit_state = state.mouse_edit_state;

  switch (mouse_edit_state.type) {
    case MouseEditState::Type::SetOn:
    case MouseEditState::Type::DeleteOn:
    case MouseEditState::Type::SetNote:
    case MouseEditState::Type::DeleteNote:
      break;
    case MouseEditState::Type::Nothing: {
      if (std::holds_alternative<MouseMeasureEdit>(
              state.mouse_edit_state.kind)) {
        int half_meas = 2 * mouse_edit_state.current_clock / clockPerMeas + 1;
        int meas = half_meas / 2;
        int left_half = half_meas % 2 == 1;
        drawFlag(&painter, (left_half ? FlagType::Repeat : FlagType::Last),
                 true, meas * clockPerMeas / state.scale.clockPerPx, FLAG_Y);
      }
      break;
    }
    case MouseEditState::Type::Seek:
      painter.fillRect(mouse_edit_state.current_clock / state.scale.clockPerPx,
                       0, 1, height,
                       QColor::fromRgb(255, 255, 255, 128 * alphaMultiplier));
      break;
    case MouseEditState::Type::Select: {
      Interval interval(mouse_edit_state.clock_int(quantizeClock) /
                        state.scale.clockPerPx);
      drawSelection(painter, interval, height, selectionAlphaMultiplier);
    } break;
  }
}

QSize MeasureView::sizeHint() const {
  return QSize(one_over_last_clock(m_client->pxtn()) /
                   m_client->editState().scale.clockPerPx,
               2 + MEASURE_NUM_BLOCK_HEIGHT + RULER_HEIGHT + SEPARATOR_OFFSET);
}

const static QBrush measureBrush(Qt::white);
const static QBrush beatBrush(QColor::fromRgb(128, 128, 128));
const static QBrush measureNumBlockBrush(QColor::fromRgb(96, 96, 96));
void MeasureView::paintEvent(QPaintEvent *) {
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
                   QColor::fromRgb(128, 0, 0));
  painter.fillRect(activeWidth, MEASURE_NUM_BLOCK_HEIGHT, width() - activeWidth,
                   RULER_HEIGHT, QColor::fromRgb(64, 0, 0));
  painter.fillRect(0,
                   MEASURE_NUM_BLOCK_HEIGHT + RULER_HEIGHT + SEPARATOR_OFFSET,
                   width(), 1, beatBrush);
  for (int beat = 0; true; ++beat) {
    int x = beat * master->get_beat_clock() /
            m_client->editState().scale.clockPerPx;
    if (x > width()) break;
    if (beat % master->get_beat_num() == 0) {
      int measure = beat / master->get_beat_num();
      painter.fillRect(x, MEASURE_NUM_BLOCK_HEIGHT, 1, size().height(),
                       measureBrush);
      if (x - lastMeasureDraw < MEASURE_NUM_BLOCK_WIDTH) continue;
      lastMeasureDraw = x;
      painter.fillRect(x, 0, 1, MEASURE_NUM_BLOCK_HEIGHT, measureBrush);
      painter.fillRect(x + 1, 0, MEASURE_NUM_BLOCK_WIDTH,
                       MEASURE_NUM_BLOCK_HEIGHT, measureNumBlockBrush);
      if (measure < activeMeas)
        drawNum(&painter, x + 1, 1, MEASURE_NUM_BLOCK_WIDTH - 1,
                beat / master->get_beat_num());
    } else
      painter.fillRect(x, MEASURE_NUM_BLOCK_HEIGHT + RULER_HEIGHT, 1, height(),
                       beatBrush);
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

  drawCurrentPlayerPosition(painter, m_moo_clock, height(),
                            m_client->editState().scale.clockPerPx, true);
  for (const auto &[uid, remote_state] : m_client->remoteEditStates()) {
    if (uid == m_client->following_uid()) continue;
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
              quantizeXOptions[adjusted_state.m_quantize_clock_idx].second),
          clockPerMeas, alphaMultiplier, selectionAlphaMultiplier);
    }
  }
  drawExistingSelection(painter, m_client->editState().mouse_edit_state,
                        m_client->editState().scale.clockPerPx, height(), 1);
  drawOngoingAction(m_client->editState(), painter, height(),
                    m_client->quantizeClock(), clockPerMeas, 1, 1);

  // Draw cursors
  for (const auto &[uid, remote_state] : m_client->remoteEditStates()) {
    if (uid == m_client->following_uid()) continue;
    if (remote_state.state.has_value()) {
      EditState state = remote_state.state.value();
      if (state.current_param_kind_idx() !=
          m_client->editState().current_param_kind_idx())
        continue;
      state.scale =
          m_client->editState().scale;  // Position according to our scale
      int unit_id = state.m_current_unit_id;
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
}

static void updateStatePositions(EditState &edit_state,
                                 const QMouseEvent *event) {
  MouseEditState &state = edit_state.mouse_edit_state;

  if (state.type == MouseEditState::Nothing ||
      state.type == MouseEditState::Seek) {
    bool shift = event->modifiers() & Qt::ShiftModifier;
    state.type = (shift ? MouseEditState::Seek : MouseEditState::Nothing);
  }

  state.current_clock =
      std::max(0., event->localPos().x() * edit_state.scale.clockPerPx);
  state.kind = MouseMeasureEdit{event->y()};
}

void MeasureView::mousePressEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }

  m_client->changeEditState([&](EditState &s) {
    s.mouse_edit_state.start_clock =
        std::max(0., event->localPos().x() * s.scale.clockPerPx);
    MouseEditState::Type &type = s.mouse_edit_state.type;
    if (event->modifiers() & Qt::ShiftModifier) {
      if (event->modifiers() & Qt::ControlModifier &&
          event->button() != Qt::RightButton)
        type = MouseEditState::Type::Select;
      else {
        if (event->button() == Qt::RightButton) m_client->deselect();
        type = MouseEditState::Type::Seek;
      }
    }
  });
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
  m_client->changeEditState([&](EditState &s) {
    if (m_client->pxtn()->Unit_Num() > 0) {
      using namespace Action;
      std::list<Primitive> actions;
      switch (s.mouse_edit_state.type) {
        case MouseEditState::SetOn:
        case MouseEditState::DeleteOn:
        case MouseEditState::SetNote:
        case MouseEditState::DeleteNote:
          break;
          // TODO: Dedup w/ the other seek / select responses
        case MouseEditState::Seek:
          if (event->button() & Qt::LeftButton)
            m_client->seekMoo(current_clock);
          break;
        case MouseEditState::Select: {
          Interval clock_int(m_client->editState().mouse_edit_state.clock_int(
              m_client->quantizeClock()));
          s.mouse_edit_state.selection.emplace(clock_int);
          break;
        }
        case MouseEditState::Nothing:
          if (std::holds_alternative<MouseMeasureEdit>(
                  s.mouse_edit_state.kind)) {
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
              if (m_client->pxtn()->master->get_play_meas() == meas)
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
    s.mouse_edit_state.type = MouseEditState::Type::Nothing;
    updateStatePositions(s, event);
  });
}

void MeasureView::wheelEvent(QWheelEvent *event) {
  handleWheelEventWithModifier(event, m_client, true);
}

void MeasureView::mouseMoveEvent(QMouseEvent *event) {
  // TODO: Change the note preview based off position.
  if (!m_client->isFollowing())
    m_client->changeEditState([&](auto &s) { updateStatePositions(s, event); });
  event->ignore();
}
