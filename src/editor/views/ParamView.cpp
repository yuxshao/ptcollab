#include "ParamView.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>

#include "ViewHelper.h"
#include "editor/ComboOptions.h"

ParamView::ParamView(PxtoneClient *client, MooClock *moo_clock, QWidget *parent)
    : QWidget(parent),
      m_client(client),
      m_anim(new Animation(this)),
      m_moo_clock(moo_clock) {
  // TODO: dedup with keyboardview
  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  updateGeometry();
  setMouseTracking(true);
  connect(m_anim, &Animation::nextFrame, [this]() { update(); });
  connect(m_client, &PxtoneClient::editStateChanged,
          [this](const EditState &s) {
            if (!(m_last_scale == s.scale)) updateGeometry();
            m_last_scale = s.scale;
          });
  connect(m_client, &PxtoneClient::measureNumChanged,
          [this]() { updateGeometry(); });
}

// TODO: Don't repeat
static int one_over_last_clock(pxtnService const *pxtn) {
  return pxtn->master->get_beat_clock() * (pxtn->master->get_meas_num() + 1) *
         pxtn->master->get_beat_num();
}

QSize ParamView::sizeHint() const {
  return QSize(one_over_last_clock(m_client->pxtn()) /
                   m_client->editState().scale.clockPerPx,
               0x20);
}

static qreal paramToY(int param, int height) {
  return (0x80 - param) * height / 0x80;
}

static qreal paramOfY(int y, int height) { return 0x80 - (y * 0x80 / height); }

void drawCursor(const EditState &state, QPainter &painter, const QColor &color,
                const QString &username, qint64 uid, int height) {
  if (!std::holds_alternative<MouseParamEdit>(state.mouse_edit_state.kind))
    return;
  const auto &param_edit_state =
      std::get<MouseParamEdit>(state.mouse_edit_state.kind);
  QPoint position(state.mouse_edit_state.current_clock / state.scale.clockPerPx,
                  paramToY(param_edit_state.current_param, height));
  drawCursor(position, painter, color, username, uid);
}

struct ParamEditInterval {
  Interval clock;
  qint32 param;
};

std::list<ParamEditInterval> lineEdit(const MouseEditState &state,
                                      int quantizeClock) {
  std::list<ParamEditInterval> ret;
  if (!std::holds_alternative<MouseParamEdit>(state.kind)) return ret;
  const auto &param_state = std::get<MouseParamEdit>(state.kind);
  Interval interval(state.clock_int(quantizeClock));
  bool reverse = state.start_clock > state.current_clock;
  for (int i = 0; interval.start + i * quantizeClock < interval.end; ++i) {
    int steps = interval.length() / quantizeClock;
    int start_clock = interval.start + i * quantizeClock;

    int param;
    if (steps == 1)
      param = param_state.start_param;
    else {
      int currStep = reverse ? (steps - 1 - i) : i;
      param = param_state.start_param +
              (param_state.current_param - param_state.start_param) * currStep /
                  (steps - 1);
    }
    ret.push_back({{start_clock, start_clock + quantizeClock}, param});
  }
  return ret;
}

static const QColor blue(QColor::fromRgb(52, 50, 85));
static const QColor darkBlue(QColor::fromRgb(26, 25, 73));
constexpr int BACKGROUND_GAPS[] = {-1000, 24, 32, 64, 96, 104, 1000};
static const QColor *GAP_COLORS[] = {&darkBlue, &darkBlue, &blue,
                                     &blue,     &darkBlue, &darkBlue};

static const QColor darkTeal(QColor::fromRgb(0, 96, 96));
static const QColor brightGreen(QColor::fromRgb(0, 240, 128));

constexpr int NUM_BACKGROUND_GAPS =
    sizeof(BACKGROUND_GAPS) / sizeof(BACKGROUND_GAPS[0]);
constexpr int WINDOW_BOUND_SLACK = 32;

void ParamView::paintEvent(QPaintEvent *event) {
  const pxtnService *pxtn = m_client->pxtn();
  Interval clockBounds = {
      qint32(event->rect().left() * m_client->editState().scale.clockPerPx) -
          WINDOW_BOUND_SLACK,
      qint32(event->rect().right() * m_client->editState().scale.clockPerPx) +
          WINDOW_BOUND_SLACK};
  QPainter painter(this);
  painter.fillRect(0, 0, size().width(), size().height(), Qt::black);

  // Draw white lines under background
  // TODO: Dedup with keyboardview
  QBrush beatBrush(QColor::fromRgb(128, 128, 128));
  QBrush measureBrush(Qt::white);
  const pxtnMaster *master = pxtn->master;
  for (int beat = 0; true; ++beat) {
    bool isMeasureLine = (beat % master->get_beat_num() == 0);
    int x = master->get_beat_clock() * beat /
            m_client->editState().scale.clockPerPx;
    if (x > size().width()) break;
    painter.fillRect(x, 0, 1, size().height(),
                     (isMeasureLine ? measureBrush : beatBrush));
  }

  // Draw param background
  for (int i = 0; i < NUM_BACKGROUND_GAPS - 1; ++i) {
    int this_y = BACKGROUND_GAPS[i] * size().height() / 0x80;
    int next_y = BACKGROUND_GAPS[i + 1] * size().height() / 0x80;
    painter.fillRect(0, this_y + 1, size().width(),
                     std::max(1, next_y - this_y - 2), *GAP_COLORS[i]);
  }

  int32_t lineHeight = 4;
  int32_t lineWidth = 2;
  {
    EVENTKIND current_kind =
        paramOptions[m_client->editState().m_current_param_kind_idx].second;
    int32_t last_value = DefaultKindValue(current_kind);
    int32_t last_clock = -1000;
    for (const EVERECORD *e = pxtn->evels->get_Records(); e != nullptr;
         e = e->next) {
      if (e->clock > clockBounds.end) break;

      if (e->kind != current_kind) continue;
      int unit_no = e->unit_no;
      qint32 unit_id = m_client->unitIdMap().noToId(unit_no);
      bool matchingUnit = (unit_id == m_client->editState().m_current_unit_id);
      if (!matchingUnit) continue;

      int32_t lastX = last_clock / m_client->editState().scale.clockPerPx;
      int32_t lastY = paramToY(last_value, size().height());
      int32_t thisX = e->clock / m_client->editState().scale.clockPerPx;
      int32_t thisY = paramToY(e->value, size().height());

      painter.fillRect(lastX, lastY - lineHeight / 2, thisX - lastX, lineHeight,
                       darkTeal);
      painter.fillRect(
          thisX, std::min(lastY, thisY) - lineHeight / 2, lineWidth,
          std::max(lastY, thisY) - std::min(lastY, thisY) + lineHeight,
          darkTeal);
      painter.fillRect(lastX, lastY - lineHeight / 2, lineWidth, lineHeight,
                       brightGreen);

      last_value = e->value;
      last_clock = e->clock;
    }
    int32_t lastX = last_clock / m_client->editState().scale.clockPerPx;
    int32_t lastY = (0x80 - last_value) * size().height() / 0x80;
    painter.fillRect(lastX, lastY - lineHeight / 2, size().width() - lastX,
                     lineHeight, darkTeal);
    painter.fillRect(lastX, lastY - lineHeight / 2, lineWidth, lineHeight,
                     brightGreen);
  }

  // draw ongoing edit
  const MouseEditState &mouse_edit_state =
      m_client->editState().mouse_edit_state;
  switch (mouse_edit_state.type) {
    case MouseEditState::Type::Nothing:
    case MouseEditState::Type::SetOn:
    case MouseEditState::Type::DeleteOn:
    case MouseEditState::Type::SetNote:
    case MouseEditState::Type::DeleteNote: {
      QColor c(brightGreen);
      c.setAlpha(mouse_edit_state.type == MouseEditState::Nothing ? 128 : 255);
      for (const ParamEditInterval &p :
           lineEdit(mouse_edit_state, m_client->quantizeClock())) {
        int x = p.clock.start / m_client->editState().scale.clockPerPx;
        int w = p.clock.length() / m_client->editState().scale.clockPerPx;
        int y = paramToY(p.param, height());
        painter.fillRect(x, y - lineHeight / 2, w, lineHeight, c);
      }
    } break;
    // TODO
    case MouseEditState::Type::Seek:
      break;
    case MouseEditState::Type::Select:
      break;
  }

  drawCurrentPlayerPosition(painter, m_moo_clock, height(),
                            m_client->editState().scale.clockPerPx, false);
  drawRepeatAndEndBars(painter, m_moo_clock,
                       m_client->editState().scale.clockPerPx, height());

  // Draw cursor
  {
    QString my_username = "";
    auto it = m_client->remoteEditStates().find(m_client->uid());
    if (it != m_client->remoteEditStates().end()) my_username = it->second.user;
    drawCursor(m_client->editState(), painter, Qt::white, my_username,
               m_client->uid(), height());
  }
}

// TODO: DEDUP
static void updateStatePositions(EditState &edit_state,
                                 const QMouseEvent *event, int height) {
  MouseEditState &state = edit_state.mouse_edit_state;
  state.current_clock =
      std::max(0., event->localPos().x() * edit_state.scale.clockPerPx);
  qint32 current_param = paramOfY(event->localPos().y(), height);
  if (!std::holds_alternative<MouseParamEdit>(state.kind))
    state.kind = MouseParamEdit{current_param, current_param};
  auto &param_edit_state = std::get<MouseParamEdit>(state.kind);

  param_edit_state.current_param = current_param;

  if (state.type == MouseEditState::Type::Nothing ||
      state.type == MouseEditState::Type::Seek) {
    state.type = (event->modifiers() & Qt::ShiftModifier
                      ? MouseEditState::Type::Seek
                      : MouseEditState::Type::Nothing);
    state.start_clock = state.current_clock;
    param_edit_state.start_param = current_param;
  }
}

void ParamView::mousePressEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }

  bool make_note_preview = false;
  m_client->changeEditState([&](EditState &s) {
    if (event->button() == Qt::RightButton)
      s.mouse_edit_state.type = MouseEditState::Type::DeleteOn;
    else {
      s.mouse_edit_state.type = MouseEditState::Type::SetOn;
      make_note_preview = true;
    }
  });

  // TODO: make note preview
}

void ParamView::mouseReleaseEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }
  if (!std::holds_alternative<MouseParamEdit>(
          m_client->editState().mouse_edit_state.kind))
    return;

  Interval clock_int(m_client->editState().mouse_edit_state.clock_int(
      m_client->quantizeClock()));

  m_client->changeEditState([&](EditState &s) {
    if (m_client->pxtn()->Unit_Num() > 0) {
      using namespace Action;
      std::list<Primitive> actions;
      EVENTKIND kind = paramOptions[s.m_current_param_kind_idx].second;
      switch (s.mouse_edit_state.type) {
        case MouseEditState::SetOn:
        case MouseEditState::DeleteOn:
        case MouseEditState::SetNote:
        case MouseEditState::DeleteNote:
          actions.push_back({kind, s.m_current_unit_id, clock_int.start,
                             Delete{clock_int.end}});
          if (s.mouse_edit_state.type == MouseEditState::SetOn ||
              s.mouse_edit_state.type == MouseEditState::SetNote) {
            for (const ParamEditInterval &p :
                 lineEdit(s.mouse_edit_state, m_client->quantizeClock())) {
              actions.push_back(
                  {kind, s.m_current_unit_id, p.clock.start, Add{p.param}});
            }
          }
          break;
          // TODO: Dedup
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
    updateStatePositions(s, event, height());
  });
}

void ParamView::wheelEvent(QWheelEvent *event) {
  handleWheelEventWithModifier(event, m_client, true);
}

void ParamView::mouseMoveEvent(QMouseEvent *event) {
  // TODO: Change the note preview based off position.
  m_client->changeEditState(
      [&](auto &s) { updateStatePositions(s, event, height()); });
  event->ignore();
}
