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
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
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

QSize MeasureView::sizeHint() const {
  return QSize(one_over_last_clock(m_client->pxtn()) /
                   m_client->editState().scale.clockPerPx,
               0x20);
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
constexpr int NUM_OFFSET_Y = 32;
static void drawNum(QPainter *painter, int x, int y, int w, int num) {
  static QPixmap numbers(":/images/images");
  int xr = x + w;
  do {
    int digit = num % 10;
    xr -= NUM_WIDTH;
    painter->drawPixmap(xr, y, NUM_WIDTH, NUM_HEIGHT, numbers,
                        NUM_OFFSET_X + digit * NUM_WIDTH, NUM_OFFSET_Y,
                        NUM_WIDTH, NUM_HEIGHT);
    num = (num - digit) / 10;
  } while (num > 0);
}

void drawOngoingAction(const EditState &state, QPainter &painter, int height,
                       int quantizeClock, double alphaMultiplier,
                       double selectionAlphaMultiplier) {
  const MouseEditState &mouse_edit_state = state.mouse_edit_state;

  switch (mouse_edit_state.type) {
    case MouseEditState::Type::Nothing:
    case MouseEditState::Type::SetOn:
    case MouseEditState::Type::DeleteOn:
    case MouseEditState::Type::SetNote:
    case MouseEditState::Type::DeleteNote:
      break;
    case MouseEditState::Type::Seek:
      painter.fillRect(mouse_edit_state.current_clock / state.scale.clockPerPx,
                       0, 1, height,
                       QColor::fromRgb(255, 255, 255, 128 * alphaMultiplier));
      break;
    case MouseEditState::Type::Select: {
      Interval interval(mouse_edit_state.clock_int(quantizeClock));
      drawSelection(painter, interval, height, selectionAlphaMultiplier);
    } break;
  }
}

constexpr int MEASURE_NUM_BLOCK_WIDTH = 27;
constexpr int MEASURE_NUM_BLOCK_HEIGHT = 10;
void MeasureView::paintEvent(QPaintEvent *) {
  const pxtnService *pxtn = m_client->pxtn();

  QPainter painter(this);
  painter.fillRect(0, 0, width(), height(), Qt::black);

  // Draw white lines under background
  // TODO: Dedup with keyboardview
  QBrush measureBrush(Qt::white);
  QBrush measureNumBlockBrush(QColor::fromRgb(96, 96, 96));
  const pxtnMaster *master = pxtn->master;
  int lastDraw = -MEASURE_NUM_BLOCK_WIDTH - 1;
  for (int measure = 0; true; ++measure) {
    int x = measure * master->get_beat_clock() * master->get_beat_num() /
            m_client->editState().scale.clockPerPx;
    if (x > width()) break;
    painter.fillRect(x, MEASURE_NUM_BLOCK_HEIGHT, 1, size().height(),
                     measureBrush);
    if (x - lastDraw < MEASURE_NUM_BLOCK_WIDTH) continue;
    lastDraw = x;
    painter.fillRect(x, 0, 1, MEASURE_NUM_BLOCK_HEIGHT, measureBrush);
    painter.fillRect(x + 1, 0, MEASURE_NUM_BLOCK_WIDTH,
                     MEASURE_NUM_BLOCK_HEIGHT, measureNumBlockBrush);
    drawNum(&painter, x + 1, 1, MEASURE_NUM_BLOCK_WIDTH - 1, measure);
  }

  drawCurrentPlayerPosition(painter, m_moo_clock, height(),
                            m_client->editState().scale.clockPerPx, true);
  for (const auto &[uid, remote_state] : m_client->remoteEditStates()) {
    if (uid == m_client->uid()) continue;
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
          m_client->quantizeClock(adjusted_state.m_quantize_clock_idx),
          alphaMultiplier, selectionAlphaMultiplier);
    }
  }
  drawExistingSelection(painter, m_client->editState().mouse_edit_state,
                        m_client->editState().scale.clockPerPx, height(), 1);
  drawOngoingAction(m_client->editState(), painter, height(),
                    m_client->quantizeClock(), 1, 1);

  // Draw cursors
  for (const auto &[uid, remote_state] : m_client->remoteEditStates()) {
    if (uid == m_client->uid()) continue;
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
    auto it = m_client->remoteEditStates().find(m_client->uid());
    if (it != m_client->remoteEditStates().end()) my_username = it->second.user;
    drawCursor(m_client->editState(), painter, Qt::white, my_username,
               m_client->uid());
  }
}

static void updateStatePositions(EditState &edit_state,
                                 const QMouseEvent *event) {
  MouseEditState &state = edit_state.mouse_edit_state;
  if (state.type == MouseEditState::Type::Nothing)
    state.type = MouseEditState::Type::Seek;

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
    if (event->modifiers() & Qt::ControlModifier &&
        event->button() != Qt::RightButton)
      type = MouseEditState::Type::Select;
    else {
      if (event->button() == Qt::RightButton) m_client->deselect();
      type = MouseEditState::Type::Seek;
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

  Interval clock_int(m_client->editState().mouse_edit_state.clock_int(
      m_client->quantizeClock()));

  EVENTKIND kind =
      paramOptions[m_client->editState().current_param_kind_idx()].second;
  if (kind == EVENTKIND_PORTAMENT)
    clock_int = m_client->editState().mouse_edit_state.clock_int_short(
        m_client->quantizeClock());
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
  });
}

void MeasureView::wheelEvent(QWheelEvent *event) {
  handleWheelEventWithModifier(event, m_client, true);
}

void MeasureView::mouseMoveEvent(QMouseEvent *event) {
  // TODO: Change the note preview based off position.
  m_client->changeEditState([&](auto &s) { updateStatePositions(s, event); });
  event->ignore();
}
