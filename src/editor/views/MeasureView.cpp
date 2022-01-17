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

  connect(m_client, &PxtoneClient::editStateChanged, this,
          &MeasureView::handleNewEditState);
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
constexpr int UNIT_EDIT_MARGIN = 1;
constexpr int UNIT_EDIT_INCREMENT = UNIT_EDIT_MARGIN + UNIT_EDIT_HEIGHT;
inline int unit_edit_y(int i) { return UNIT_EDIT_Y + UNIT_EDIT_INCREMENT * i; }

struct UnitDrawParams {
  std::vector<int> ys;
  const Brush *brush;
};
struct UnitDrawParamsMap {
  std::map<int, UnitDrawParams> no_to_params;
  std::vector<MeasureUnitEdit> rows;
};

// TODO: Cache this on edit state change only
// Helper index for going from pinned no / id -> draw location / brush, and back
UnitDrawParamsMap make_draw_params_map(const EditState &e, const NoIdMap &m) {
  std::map<int, UnitDrawParams> no_to_params;
  std::vector<MeasureUnitEdit> rows;

  for (int unit_id : e.m_pinned_unit_ids) {
    std::optional<int> maybe_unit_no = m.idToNo(unit_id);
    if (maybe_unit_no.has_value()) {
      UnitDrawParams &p = no_to_params[maybe_unit_no.value()];
      p.brush = &brushes[nonnegative_modulo(unit_id, NUM_BRUSHES)];
    }
  }

  int row = 0;
  for (auto &[unit_no, p] : no_to_params) {
    p.ys.push_back(unit_edit_y(row) + UNIT_EDIT_HEIGHT / 2);
    rows.push_back(MeasureUnitEdit{m.noToId(unit_no)});
    ++row;
  }

  rows.push_back(MeasureUnitEdit{std::nullopt});
  std::optional<int> maybe_unit_no = m.idToNo(e.m_current_unit_id);
  if (maybe_unit_no.has_value()) {
    UnitDrawParams &p = no_to_params[maybe_unit_no.value()];
    p.brush = &brushes[nonnegative_modulo(e.m_current_unit_id, NUM_BRUSHES)];
    p.ys.push_back(unit_edit_y(row) + UNIT_EDIT_HEIGHT / 2);
  }

  return UnitDrawParamsMap{no_to_params, rows};
}

UnitDrawParamsMap make_draw_params_map(const PxtoneClient *c) {
  return make_draw_params_map(c->editState(), c->unitIdMap());
}

void drawCursor(const EditState &state,
                const UnitDrawParamsMap &draw_params_map,
                const NoIdMap &unit_id_map, QPainter &painter,
                const QColor &color, const QString &username, qint64 uid) {
  if (!std::holds_alternative<MouseMeasureEdit>(state.mouse_edit_state.kind))
    return;
  MouseMeasureEdit m = std::get<MouseMeasureEdit>(state.mouse_edit_state.kind);
  int offset_y = m.offset_y;
  std::optional<int> y;
  std::visit(
      overloaded{[&](MeasureRibbonEdit &) { y = m.offset_y; },
                 [&](MeasureUnitEdit &m) {
                   std::optional<int> no;
                   bool pinned = m.pinned_unit_id.has_value();
                   if (pinned)
                     no = unit_id_map.idToNo(m.pinned_unit_id.value());
                   else
                     no = unit_id_map.idToNo(state.m_current_unit_id);
                   if (no.has_value() &&
                       draw_params_map.no_to_params.count(no.value()) > 0) {
                     auto &ys = draw_params_map.no_to_params.at(no.value()).ys;
                     // There are multiple places you could draw the
                     // cursor. If it's pinned on their end draw it in
                     // the pinned slot on yours if you can (front).
                     y = (pinned ? ys.front() : ys.back()) + offset_y -
                         UNIT_EDIT_HEIGHT / 2;
                   }
                 }},
      m.kind);

  if (y.has_value()) {
    QPoint p(state.mouse_edit_state.current_clock / state.scale.clockPerPx,
             y.value());
    drawCursor(p, painter, color, username, uid);
  }
}

void drawOngoingAction(const EditState &state,
                       const UnitDrawParamsMap &unit_draw_params_map,
                       const NoIdMap &unit_id_map, QPainter &painter,
                       int height, int quantizeClock, int clockPerMeas,
                       std::optional<int> nowNoWrap, const pxtnMaster *master,
                       double alphaMultiplier,
                       double selectionAlphaMultiplier) {
  const MouseEditState &mouse_edit_state = state.mouse_edit_state;

  auto drawVelAction = [&](const Interval &interval, qint32 velocity,
                           int alpha) {
    const auto &m = std::get<MouseMeasureEdit>(state.mouse_edit_state.kind);
    if (!std::holds_alternative<MeasureUnitEdit>(m.kind)) return;
    const auto &measure_unit_edit = std::get<MeasureUnitEdit>(m.kind);
    int unit_id =
        measure_unit_edit.pinned_unit_id.value_or(state.m_current_unit_id);
    auto no = unit_id_map.idToNo(unit_id);
    if (!no.has_value() ||
        unit_draw_params_map.no_to_params.count(no.value()) == 0)
      return;
    const Brush &brush = brushes[nonnegative_modulo(unit_id, NUM_BRUSHES)];
    for (int y : unit_draw_params_map.no_to_params.at(no.value()).ys) {
      painter.fillRect(
          interval.start, y + 3 - UNIT_EDIT_HEIGHT / 2, interval.length(),
          UNIT_EDIT_HEIGHT - 6,
          brush.toQColor(velocity, false, alpha * alphaMultiplier));
    }
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
          auto &[kind, _] =
              std::get<MouseMeasureEdit>(state.mouse_edit_state.kind);
          if (std::holds_alternative<MeasureRibbonEdit>(kind)) {
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
               unit_edit_y(1 + m_client->editState().m_pinned_unit_ids.size()));
}

void MeasureView::handleNewEditState(const EditState &) {
  if (size() != sizeHint()) {
    updateGeometry();

    // Used to change the actual scrollarea size...
    emit heightChanged(sizeHint().height());
  }
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

  double scaleX = m_client->editState().scale.clockPerPx;
  Interval clockBounds = {
      qint32(e->rect().left() * scaleX) - WINDOW_BOUND_SLACK,
      qint32(e->rect().right() * scaleX) + WINDOW_BOUND_SLACK};
  UnitDrawParamsMap unit_draw_params_map(make_draw_params_map(m_client));
  for (uint i = 0; i < unit_draw_params_map.rows.size(); ++i)
    painter.fillRect(0, unit_edit_y(i), width(), UNIT_EDIT_HEIGHT,
                     StyleEditor::palette.MeasureUnitEdit);
  std::map<int, Interval> last_on_by_no;
  std::map<int, int> last_vel_by_no;
  std::optional<Interval> selection(
      m_client->editState().mouse_edit_state.selection);

  std::set<int> selected_unit_nos = m_client->selectedUnitNos();

  auto drawLastOn = [&](int no, bool erase) {
    if (last_on_by_no.count(no) > 0) {
      const Interval &i = last_on_by_no[no];
      int vel = (last_vel_by_no.count(no) > 0 ? last_vel_by_no[no]
                                              : EVENTDEFAULT_VELOCITY);
      int x = i.start / scaleX;
      int w = int(i.end / scaleX) - x;
      const Brush *brush = unit_draw_params_map.no_to_params[no].brush;
      QColor c = brush->toQColor(vel, i.contains(m_moo_clock->now()), 255);
      for (int y : unit_draw_params_map.no_to_params[no].ys)
        fillUnitBullet(painter, x, y, w, c);
      if (selected_unit_nos.count(no) > 0 && selection.has_value()) {
        Interval selection_segment = interval_intersect(selection.value(), i);
        if (!selection_segment.empty()) {
          painter.setPen(brush->toQColor(EVENTDEFAULT_VELOCITY, true, 255));
          int x = selection_segment.start / scaleX;
          int w = int(selection_segment.end / scaleX) - x;
          for (int y : unit_draw_params_map.no_to_params[no].ys)
            drawUnitBullet(painter, x, y, w);
        }
      }
      if (erase) last_on_by_no.erase(no);
    }
  };

  for (const EVERECORD *e = pxtn->evels->get_Records(); e != nullptr;
       e = e->next) {
    if (e->clock > clockBounds.end) break;
    if (unit_draw_params_map.no_to_params.count(e->unit_no) > 0) {
      if (e->kind == EVENTKIND_ON) {
        drawLastOn(e->unit_no, true);
        last_on_by_no[e->unit_no] = Interval{e->clock, e->clock + e->value};
      }
      if (e->kind == EVENTKIND_VELOCITY) {
        if (last_on_by_no.count(e->unit_no) > 0 &&
            last_on_by_no[e->unit_no].start < e->clock)
          drawLastOn(e->unit_no, true);
        last_vel_by_no[e->unit_no] = e->value;
      }
    }
  }
  for (auto it = last_on_by_no.begin(); it != last_on_by_no.end(); ++it)
    drawLastOn(it->first, false);

  // Draw text labels
  if (Settings::PinnedUnitLabels::get()) {
    QPixmap textLabelLayer(e->rect().size());
    textLabelLayer.fill(Qt::transparent);
    QPainter textLabelPainter(&textLabelLayer);
    textLabelPainter.translate(-e->rect().topLeft());
    textLabelPainter.setFont(QFont("Sans serif", Settings::TextSize::get()));
    textLabelPainter.setPen(Qt::white);
    int maxTextLabelWidth = 0;
    for (uint i = 0; i < unit_draw_params_map.rows.size(); ++i) {
      if (!unit_draw_params_map.rows[i].pinned_unit_id.has_value()) continue;
      int unit_id = unit_draw_params_map.rows[i].pinned_unit_id.value();
      std::optional<int> unit_no = m_client->unitIdMap().idToNo(unit_id);
      if (!unit_no.has_value()) continue;
      QString unit_name =
          shift_jis_codec->toUnicode(m_client->pxtn()
                                         ->Unit_Get(unit_no.value())
                                         ->get_name_buf_jis(nullptr));
      QColor bg = StyleEditor::palette.MeasureUnitEdit;
      bg.setAlphaF(0.25);
      textLabelPainter.setPen(bg);
      constexpr int x_padding = 5;
      for (int x = -2; x <= 2; ++x)
        for (int y = -1; y <= 1; ++y)
          textLabelPainter.drawText(
              -pos().x() + x_padding + x, unit_edit_y(i) + y, 10000000,
              UNIT_EDIT_HEIGHT, Qt::AlignVCenter, unit_name);
      textLabelPainter.setPen(Qt::white);
      QRect bbox;
      textLabelPainter.drawText(-pos().x() + x_padding, unit_edit_y(i),
                                10000000, UNIT_EDIT_HEIGHT, Qt::AlignVCenter,
                                unit_name, &bbox);
      if (bbox.width() > maxTextLabelWidth) maxTextLabelWidth = bbox.width();
    }
    double textLabelAlpha = 1;
    const MouseEditState &mouse = m_client->editState().mouse_edit_state;
    if (std::holds_alternative<MouseMeasureEdit>(mouse.kind)) {
      int dx = (mouse.start_clock - clockBounds.start) /
               m_client->editState().scale.clockPerPx;
      textLabelAlpha =
          0.1 + 0.9 * clamp(dx - maxTextLabelWidth - 20, 0, 100) / 100;
    }
    painter.setOpacity(textLabelAlpha);
    painter.drawPixmap(e->rect(), textLabelLayer, textLabelLayer.rect());
    painter.setOpacity(1);
  }

  drawLastSeek(painter, m_client, height(), true);
  drawCurrentPlayerPosition(painter, m_moo_clock, height(),
                            m_client->editState().scale.clockPerPx, true);
  for (const auto &[uid, remote_state] : m_client->remoteEditStates()) {
    if (uid == m_client->following_uid() || uid == m_client->uid()) continue;
    if (remote_state.state.has_value()) {
      EditState adjusted_state(remote_state.state.value());
      adjusted_state.scale = m_client->editState().scale;

      bool same_unit = adjusted_state.m_current_unit_id ==
                       m_client->editState().m_current_unit_id;
      double selectionAlphaMultiplier = (same_unit ? 0.5 : 0.3);
      drawExistingSelection(painter, adjusted_state.mouse_edit_state,
                            adjusted_state.scale.clockPerPx, height(),
                            selectionAlphaMultiplier);

      drawOngoingAction(
          adjusted_state, unit_draw_params_map, m_client->unitIdMap(), painter,
          height(),
          m_client->quantizeClock(
              quantizeXOptions()[adjusted_state.m_quantize_clock_idx].second),
          clockPerMeas, std::nullopt, m_client->pxtn()->master, 0.7,
          selectionAlphaMultiplier);
    }
  }
  drawExistingSelection(painter, m_client->editState().mouse_edit_state,
                        m_client->editState().scale.clockPerPx, height(), 1);
  drawOngoingAction(m_client->editState(), unit_draw_params_map,
                    m_client->unitIdMap(), painter, height(),
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
      drawCursor(state, unit_draw_params_map, m_client->unitIdMap(), painter,
                 color, remote_state.user, uid);
    }
  }

  {
    QString my_username = "";
    auto it = m_client->remoteEditStates().find(m_client->following_uid());
    if (it != m_client->remoteEditStates().end()) my_username = it->second.user;
    drawCursor(m_client->editState(), unit_draw_params_map,
               m_client->unitIdMap(), painter, StyleEditor::palette.Cursor,
               my_username, m_client->following_uid());
  }
}

static void updateStatePositions(EditState &edit_state,
                                 const UnitDrawParamsMap &unit_draw_params_map,
                                 const QMouseEvent *event) {
  MouseEditState &state = edit_state.mouse_edit_state;

  state.current_clock =
      std::max(0., event->localPos().x() * edit_state.scale.clockPerPx);
  switch (state.type) {
    case MouseEditState::Nothing:
    case MouseEditState::Seek:
    case MouseEditState::Select: {
      if (state.type != MouseEditState::Select) {
        state.start_clock = state.current_clock;
        bool shift = event->modifiers() & Qt::ShiftModifier;
        state.type = (shift ? MouseEditState::Seek : MouseEditState::Nothing);
      }

      int y = event->y();
      if (y < UNIT_EDIT_Y)
        state.kind = MouseMeasureEdit{MeasureRibbonEdit{}, y};
      else {
        int row = (y - UNIT_EDIT_Y) / UNIT_EDIT_INCREMENT;
        int offset_y = (y - UNIT_EDIT_Y) % UNIT_EDIT_INCREMENT;
        const auto &rows = unit_draw_params_map.rows;
        state.kind = MouseMeasureEdit{
            (int(rows.size()) > row ? rows[row] : MeasureUnitEdit{}), offset_y};
      }
    } break;

    case MouseEditState::SetNote:
    case MouseEditState::SetOn:
    case MouseEditState::DeleteNote:
    case MouseEditState::DeleteOn:
      if (std::holds_alternative<MouseMeasureEdit>(state.kind))
        std::get<MouseMeasureEdit>(state.kind).offset_y = UNIT_EDIT_HEIGHT / 2;
      break;
  }
}

void MeasureView::mousePressEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }

  m_client->changeEditState(
      [&](EditState &s) {
        updateStatePositions(s, make_draw_params_map(m_client), event);
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
        } else {
          auto &[kind, offset_y] =
              std::get<MouseMeasureEdit>(s.mouse_edit_state.kind);
          if (std::holds_alternative<MeasureUnitEdit>(kind)) {
            if (event->button() == Qt::LeftButton) {
              type = MouseEditState::Type::SetOn;
              qint32 clock = quantize(s.mouse_edit_state.current_clock,
                                      m_client->quantizeClock());
              std::optional<int> unit_no = m_client->unitIdMap().idToNo(
                  std::get<MeasureUnitEdit>(kind).pinned_unit_id.value_or(
                      s.m_current_unit_id));
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
            case MouseEditState::SetNote:
            case MouseEditState::DeleteOn:
            case MouseEditState::DeleteNote: {
              if (!std::holds_alternative<MouseMeasureEdit>(
                      s.mouse_edit_state.kind))
                break;
              auto mouse_measure_edit =
                  std::get<MouseMeasureEdit>(s.mouse_edit_state.kind);
              if (!std::holds_alternative<MeasureUnitEdit>(
                      mouse_measure_edit.kind))
                break;
              int unit_id = std::get<MeasureUnitEdit>(mouse_measure_edit.kind)
                                .pinned_unit_id.value_or(s.m_current_unit_id);
              std::optional<int> maybe_unit_no =
                  m_client->unitIdMap().idToNo(unit_id);
              if (maybe_unit_no.has_value()) {
                if (s.mouse_edit_state.type == MouseEditState::SetOn ||
                    s.mouse_edit_state.type == MouseEditState::SetNote) {
                  actions.push_back({EVENTKIND_ON, unit_id, clock_int.start,
                                     Delete{clock_int.end}});
                  actions.push_back({EVENTKIND_VELOCITY, unit_id,
                                     clock_int.start, Delete{clock_int.end}});
                  actions.push_back({EVENTKIND_ON, unit_id, clock_int.start,
                                     Add{clock_int.length()}});
                  actions.push_back(
                      {EVENTKIND_VELOCITY, unit_id, clock_int.start,
                       Add{qint32(round(s.mouse_edit_state.base_velocity))}});
                } else {
                  actions.push_back({EVENTKIND_ON, unit_id, clock_int.start,
                                     Delete{clock_int.end}});
                  actions.push_back({EVENTKIND_VELOCITY, unit_id,
                                     clock_int.start, Delete{clock_int.end}});
                }
              }
            } break;
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
                if (std::holds_alternative<MeasureRibbonEdit>(
                        std::get<MouseMeasureEdit>(s.mouse_edit_state.kind)
                            .kind))
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
        updateStatePositions(s, make_draw_params_map(m_client), event);
      },
      false);
}

void MeasureView::moveEvent(QMoveEvent *e) {
  update();
  QWidget::moveEvent(e);
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
    m_client->changeEditState(
        [&](auto &s) {
          updateStatePositions(s, make_draw_params_map(m_client), event);
        },
        true);
  if (m_audio_note_preview != nullptr)
    m_audio_note_preview->processEvent(
        EVENTKIND_VELOCITY,
        qint32(round(m_client->editState().mouse_edit_state.base_velocity)));
  event->ignore();
}
