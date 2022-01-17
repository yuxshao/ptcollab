#include "ParamView.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>

#include "ViewHelper.h"
#include "editor/ComboOptions.h"
#include "editor/Settings.h"
#include "editor/StyleEditor.h"

ParamView::ParamView(PxtoneClient *client, MooClock *moo_clock, QWidget *parent)
    : QWidget(parent),
      m_client(client),
      m_anim(new Animation(this)),
      m_moo_clock(moo_clock),
      m_audio_note_preview(nullptr),
      m_woice_menu(new QMenu(this)),
      m_last_woice_menu_preview_id(-1) {
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
  connect(m_client->controller(), &PxtoneController::measureNumChanged, this,
          &QWidget::updateGeometry);
  connect(m_woice_menu, &QMenu::hovered, [this](QAction *action) {
    bool ok = true;
    int id = action->data().toInt(&ok);
    if (!ok) {
      qWarning() << "Invalid woice menu action woice ID";
      return;
    }
    if (m_last_woice_menu_preview_id == id &&
        m_last_woice_menu_preview_time.elapsed() < 1500)
      return;
    m_last_woice_menu_preview_time.restart();
    m_last_woice_menu_preview_id = id;
    auto maybe_unit_no =
        m_client->unitIdMap().idToNo(m_client->editState().m_current_unit_id);
    std::optional<qint32> maybe_woice_no =
        m_client->controller()->woiceIdMap().idToNo(id);
    if (maybe_woice_no.has_value() && maybe_unit_no.has_value()) {
      EVERECORD e;
      e.kind = EVENTKIND_VOICENO;
      /// Since woice changes reset the unit key, I added a hack so
      /// that the sound engine will interpret negative values the same as
      /// positive but without resetting.
      e.value = -maybe_woice_no.value() - 1;
      m_audio_note_preview = std::make_unique<NotePreview>(
          m_client->pxtn(), &m_client->moo()->params, maybe_unit_no.value(),
          m_client->editState().mouse_edit_state.start_clock, 48000,
          std::list<EVERECORD>({e}), m_client->audioState()->bufferSize(),
          Settings::ChordPreview::get(), this);
    }
  });
}

QSize ParamView::sizeHint() const {
  return QSize(one_over_last_clock(m_client->pxtn()) /
                   m_client->editState().scale.clockPerPx,
               0x20);
}

constexpr double min_tuning = -1.0 / 24, max_tuning = 1.0 / 24;
static qreal paramToY(int param, EVENTKIND current_kind, int height) {
  switch (current_kind) {
    case EVENTKIND_TUNING: {
      double tuning = log2(*((float *)&param));
      return height -
             (tuning - min_tuning) / (max_tuning - min_tuning) * height;
    }
    case EVENTKIND_GROUPNO:
      return height - param * height / (pxtnMAX_TUNEGROUPNUM - 1);
    default:
      return (0x80 - param) * height / 0x80;
  }
}

static qreal paramOfY(int y, EVENTKIND current_kind, int height, bool snap) {
  if (y > height) y = height;
  if (y < 0) y = 0;
  switch (current_kind) {
    case EVENTKIND_TUNING: {
      double proportion;
      if (!snap)
        proportion = (height - y + 0.0) / height;
      else
        proportion =
            int(0x10 - (y * 0x10 + height / 2) / height) / (0x10 + 0.0);
      float tuning = exp2(proportion * (max_tuning - min_tuning) + min_tuning);
      return *((int32_t *)&tuning);
    }
    case EVENTKIND_GROUPNO:
      return int((pxtnMAX_TUNEGROUPNUM - 1) * (1 - (y + 0.0) / height) + 0.5);
    default:
      if (!snap)
        return 0x80 - (y * 0x80 + height / 2) / height;
      else
        return (0x10 - (y * 0x10 + height / 2) / height) * 8;
  }
}

void drawCursor(const EditState &state, QPainter &painter, const QColor &color,
                const QString &username, qint64 uid, EVENTKIND current_kind,
                int height) {
  if (!std::holds_alternative<MouseParamEdit>(state.mouse_edit_state.kind))
    return;
  const auto &param_edit_state =
      std::get<MouseParamEdit>(state.mouse_edit_state.kind);
  QPoint position(
      state.mouse_edit_state.current_clock / state.scale.clockPerPx,
      paramToY(param_edit_state.current_param, current_kind, height));
  drawCursor(position, painter, color, username, uid);
}

struct ParamEditInterval {
  Interval clock;
  qint32 param;
};

std::list<ParamEditInterval> lineEdit(const MouseEditState &state,
                                      EVENTKIND current_kind,
                                      int quantizeClock) {
  std::list<ParamEditInterval> ret;
  if (!std::holds_alternative<MouseParamEdit>(state.kind)) return ret;
  const auto &param_state = std::get<MouseParamEdit>(state.kind);
  Interval interval(state.clock_int(quantizeClock));

  // use y because param space might not be linear
  constexpr int arbitrary_h = 4096;
  qreal startY = paramToY(param_state.start_param, current_kind, arbitrary_h);
  qreal endY = paramToY(param_state.current_param, current_kind, arbitrary_h);
  bool reverse = state.start_clock > state.current_clock;
  for (int i = 0; interval.start + i * quantizeClock < interval.end; ++i) {
    int steps = interval.length() / quantizeClock;
    int start_clock = interval.start + i * quantizeClock;

    qreal y;
    if (steps == 1)
      y = startY;
    else {
      int currStep = reverse ? (steps - 1 - i) : i;
      y = startY + (endY - startY) * currStep / (steps - 1);
    }
    int param = paramOfY(y, current_kind, arbitrary_h, false);
    ret.push_back({{start_clock, start_clock + quantizeClock}, param});
  }
  return ret;
}

constexpr int BACKGROUND_GAPS[] = {-1000, 24, 32, 64, 96, 104, 1000};
static const QColor *GAP_COLORS[] = {
    &StyleEditor::palette.ParamDarkBlue, &StyleEditor::palette.ParamDarkBlue,
    &StyleEditor::palette.ParamBlue,     &StyleEditor::palette.ParamBlue,
    &StyleEditor::palette.ParamDarkBlue, &StyleEditor::palette.ParamDarkBlue};

constexpr int NUM_BACKGROUND_GAPS =
    sizeof(BACKGROUND_GAPS) / sizeof(BACKGROUND_GAPS[0]);
constexpr int arbitrarily_tall = 1000;

struct Event {
  int clock, value;
};

constexpr int32_t lineHeight = 4;
constexpr int32_t lineWidth = 2;
constexpr int32_t tailRowHeight = 8;
static void drawLastVoiceNoEvent(QPainter &painter, int height,
                                 const Event &last, const Event &curr,
                                 qreal clockPerPx, const QColor &onColor,
                                 const pxtnService *pxtn) {
  int32_t lastX = last.clock / clockPerPx;
  QPainterPath path;
  constexpr int s = 4;
  path.moveTo(lastX, height / 2 - s);
  path.lineTo(lastX + s, height / 2);
  path.lineTo(lastX, height / 2 + s);
  path.closeSubpath();
  painter.fillPath(path, onColor);

  std::shared_ptr<const pxtnWoice> woice = pxtn->Woice_Get(last.value);
  if (woice != nullptr) {
    int32_t thisX = curr.clock / clockPerPx;
    QColor font = StyleEditor::palette.ParamFont;
    font.setAlpha(onColor.alpha());
    painter.setPen(font);
    painter.setFont(QFont("Sans serif", Settings::TextSize::get()));
    painter.drawText(
        lastX + s, height / 2, thisX - lastX - s, 10000, Qt::AlignTop,
        shift_jis_codec->toUnicode(woice->get_name_buf_jis(nullptr)));
  }
}
static void drawLastEvent(QPainter &painter, EVENTKIND current_kind, int height,
                          const Event &last, const Event &curr,
                          qreal clockPerPx, const QColor &onColor,
                          int unitOffset, int numUnits) {
  if (onColor.alpha() == 0) return;
  int32_t thisX = curr.clock / clockPerPx;

  if (!Evelist_Kind_IsTail(current_kind) && current_kind != EVENTKIND_VOICENO) {
    int32_t thisY = paramToY(curr.value, current_kind, height);
    int32_t lastX = last.clock / clockPerPx;
    int32_t lastY = paramToY(last.value, current_kind, height);
    // Horizontal line to thisX
    painter.fillRect(lastX + lineWidth, lastY - lineHeight / 2,
                     thisX - lastX - lineWidth, lineHeight, onColor);
    // Vertical line to thisY
    painter.fillRect(
        thisX, std::min(lastY, thisY) - lineHeight / 2, lineWidth,
        std::max(lastY, thisY) - std::min(lastY, thisY) + lineHeight, onColor);
    if (unitOffset == 0) {
      // Highlight at lastX
      QColor fadedWhite = StyleEditor::palette.ParamFadedWhite;
      fadedWhite.setAlpha(onColor.alpha());
      painter.fillRect(lastX, lastY - lineHeight / 2, lineWidth, lineHeight,
                       fadedWhite);
      if (current_kind == EVENTKIND_GROUPNO) {
        painter.setPen(fadedWhite);
        painter.setFont(QFont("Sans serif", Settings::TextSize::get()));
        painter.drawText(lastX + lineWidth + 1, lastY, arbitrarily_tall,
                         arbitrarily_tall, Qt::AlignTop,
                         QString("%1").arg(last.value));
      }
    }
  } else {
    int32_t w = curr.value / clockPerPx;
    int num_layer = 1;
    for (int i = -num_layer; i <= num_layer; ++i) {
      QColor color(onColor);
      color.setAlpha(color.alpha() * (num_layer + 1 - abs(i)) /
                     (num_layer + 1));
      int32_t y = height / 2 + (unitOffset + i * numUnits) * tailRowHeight;
      fillUnitBullet(painter, thisX, y, w, color);
    }
  }
}
static void drawOngoingEdit(QPainter &painter, const MouseEditState &state,
                            EVENTKIND current_kind, int quantizeClock,
                            qreal clockPerPx, int height,
                            double alphaMultiplier,
                            double selectionAlphaMultiplier, int unitOffset) {
  switch (state.type) {
    case MouseEditState::Type::Nothing:
    case MouseEditState::Type::SetOn:
    case MouseEditState::Type::DeleteOn:
    case MouseEditState::Type::SetNote:
    case MouseEditState::Type::DeleteNote: {
      if (!std::holds_alternative<MouseParamEdit>(state.kind)) break;
      QColor c = StyleEditor::palette.ParamBrightGreen;
      c.setAlpha(alphaMultiplier *
                 (state.type == MouseEditState::Nothing ? 128 : 255));
      if (!Evelist_Kind_IsTail(current_kind) &&
          current_kind != EVENTKIND_VOICENO) {
        for (const ParamEditInterval &p :
             lineEdit(state, current_kind, quantizeClock)) {
          int x = p.clock.start / clockPerPx;
          int w = p.clock.length() / clockPerPx;
          int y = paramToY(p.param, current_kind, height);
          painter.fillRect(x, y - lineHeight / 2, w, lineHeight, c);
        }
      } else {
        Interval interval = (current_kind == EVENTKIND_PORTAMENT
                                 ? state.clock_int_short(quantizeClock)
                                 : state.clock_int(quantizeClock));
        painter.fillRect(
            interval.start / clockPerPx,
            height / 2 - lineHeight / 2 + unitOffset * tailRowHeight,
            std::max(1.0, interval.length() / clockPerPx), lineHeight, c);
      }
    } break;
    case MouseEditState::Type::Seek: {
      QColor color = StyleEditor::palette.Playhead;
      color.setAlpha(StyleEditor::palette.Playhead.alpha() * alphaMultiplier /
                     2);
      painter.fillRect(
          state.current_clock / clockPerPx, 0, 1, height,
          QColor::fromRgb(
              color.red(), color.green(), color.blue(),
              128 * alphaMultiplier));  // I can't find where this is used. It
                                        // was white originally, so I will have
                                        // it use FadedWhite
      break;
    }
    case MouseEditState::Type::Select: {
      Interval interval(state.clock_int(quantizeClock) / clockPerPx);
      drawSelection(painter, interval, height, selectionAlphaMultiplier);
    } break;
  }
}
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
  QBrush beatBrush = StyleEditor::palette.ParamBeat;
  QBrush measureBrush = StyleEditor::palette.ParamMeasure;
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

  EVENTKIND current_kind =
      paramOptions()[m_client->editState().current_param_kind_idx()].second;

  QPixmap thisUnit(event->rect().size());
  thisUnit.fill(Qt::transparent);
  int current_unit_no = 0;
  qreal clockPerPx = m_client->editState().scale.clockPerPx;
  QPainter thisUnitPainter(&thisUnit);
  {
    thisUnitPainter.translate(-event->rect().topLeft());
    std::vector<QColor> colors;
    std::vector<Event> lastEvents;
    std::vector<QPainter *> painters;
    colors.reserve(m_client->pxtn()->Unit_Num());
    painters.reserve(m_client->pxtn()->Unit_Num());
    lastEvents.reserve(m_client->pxtn()->Unit_Num());
    for (int i = 0; i < m_client->pxtn()->Unit_Num(); ++i) {
      lastEvents.emplace_back(Event{-1000, DefaultKindValue(current_kind)});
      int unit_id = m_client->unitIdMap().noToId(i);
      colors.push_back(
          brushes[nonnegative_modulo(unit_id, NUM_BRUSHES)].toQColor(108, false,
                                                                     255));
      int h, s, l, a;
      colors.rbegin()->getHsl(&h, &s, &l, &a);
      if (m_client->editState().m_current_unit_id != unit_id) {
        if (m_client->pxtn()->Unit_Get(i)->get_visible())
          a *= 0.3;
        else
          a *= 0;
        painters.push_back(&painter);
      } else {
        painters.push_back(&thisUnitPainter);
        current_unit_no = i;
      }
      colors.rbegin()->setHsl(h, s, l * 3 / 4, a);
    }

    for (const EVERECORD *e = pxtn->evels->get_Records(); e != nullptr;
         e = e->next) {
      if (e->clock > clockBounds.end) break;
      if (e->kind != current_kind) continue;
      int unit_no = e->unit_no;

      Event curr{e->clock, e->value};
      if (current_kind != EVENTKIND_VOICENO)
        drawLastEvent(*painters[unit_no], current_kind, height(),
                      lastEvents[unit_no], curr, clockPerPx, colors[unit_no],
                      unit_no - current_unit_no, m_client->pxtn()->Unit_Num());
      else if (unit_no == current_unit_no)
        drawLastVoiceNoEvent(*painters[unit_no], height(), lastEvents[unit_no],
                             curr, clockPerPx, colors[unit_no],
                             m_client->pxtn());
      lastEvents[unit_no] = curr;
    }
    for (int unit_no = 0; unit_no < m_client->pxtn()->Unit_Num(); ++unit_no) {
      Event curr = lastEvents[unit_no];
      curr.clock = (width() + 50) * clockPerPx;
      if (current_kind != EVENTKIND_VOICENO)
        drawLastEvent(*painters[unit_no], current_kind, height(),
                      lastEvents[unit_no], curr, clockPerPx, colors[unit_no],
                      unit_no - current_unit_no, m_client->pxtn()->Unit_Num());
      else if (unit_no == current_unit_no)
        drawLastVoiceNoEvent(*painters[unit_no], height(), lastEvents[unit_no],
                             curr, clockPerPx, colors[unit_no],
                             m_client->pxtn());
    }
  }

  // draw ongoing edit
  for (const auto &[uid, remote_state] : m_client->remoteEditStates()) {
    if (uid == m_client->following_uid() || uid == m_client->uid()) continue;
    if (remote_state.state.has_value()) {
      const EditState &state = remote_state.state.value();
      if (state.current_param_kind_idx() !=
          m_client->editState().current_param_kind_idx())
        continue;
      QPainter *this_painter = &thisUnitPainter;
      double alphaMultiplier = 0.7, selectionAlphaMultiplier = 0.5;
      if (state.m_current_unit_id != m_client->editState().m_current_unit_id) {
        alphaMultiplier = 0.3;
        selectionAlphaMultiplier = 0.3;
        this_painter = &painter;
      }
      int unit_no = m_client->unitIdMap()
                        .idToNo(state.m_current_unit_id)
                        .value_or(current_unit_no);
      // TODO: be able to see others' param selections too.
      drawOngoingEdit(
          *this_painter, state.mouse_edit_state, current_kind,
          m_client->quantizeClock(
              quantizeXOptions()[state.m_quantize_clock_idx].second),
          clockPerPx, height(), alphaMultiplier, selectionAlphaMultiplier,
          unit_no - current_unit_no);
    }
  }
  painter.drawPixmap(event->rect(), thisUnit, thisUnit.rect());

  if (m_client->clipboard()->kindIsCopied(current_kind))
    drawExistingSelection(painter, m_client->editState().mouse_edit_state,
                          clockPerPx, size().height(), 1);
  drawOngoingEdit(painter, m_client->editState().mouse_edit_state, current_kind,
                  m_client->quantizeClock(), clockPerPx, height(), 1, 1, 0);

  drawLastSeek(painter, m_client, height(), false);
  drawCurrentPlayerPosition(painter, m_moo_clock, height(), clockPerPx, false);
  drawRepeatAndEndBars(painter, m_moo_clock, clockPerPx, height());

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
      drawCursor(state, painter, color, remote_state.user, uid, current_kind,
                 height());
    }
  }
  {
    QString my_username = "";
    auto it = m_client->remoteEditStates().find(m_client->following_uid());
    if (it != m_client->remoteEditStates().end()) my_username = it->second.user;
    drawCursor(m_client->editState(), painter, StyleEditor::palette.Cursor,
               my_username, m_client->following_uid(), current_kind, height());
  }
}

// TODO: DEDUP
static void updateStatePositions(EditState &edit_state,
                                 const QMouseEvent *event,
                                 EVENTKIND current_kind, int height) {
  MouseEditState &state = edit_state.mouse_edit_state;
  state.current_clock =
      std::max(0., event->localPos().x() * edit_state.scale.clockPerPx);
  bool snap = event->modifiers() & Qt::ControlModifier;
  qint32 current_param =
      paramOfY(event->localPos().y(), current_kind, height, snap);
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
  m_client->changeEditState(
      [&](EditState &s) {
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
          if (event->button() == Qt::RightButton)
            type = MouseEditState::Type::DeleteOn;
          else {
            type = MouseEditState::Type::SetOn;
            make_note_preview = true;
          }
        }
        if (make_note_preview) {
          auto maybe_unit_no = m_client->unitIdMap().idToNo(
              m_client->editState().m_current_unit_id);
          if (maybe_unit_no != std::nullopt &&
              std::holds_alternative<MouseParamEdit>(s.mouse_edit_state.kind)) {
            int unit_no = maybe_unit_no.value();
            EVERECORD e;
            e.kind =
                paramOptions()[m_client->editState().current_param_kind_idx()]
                    .second;
            e.value =
                std::get<MouseParamEdit>(s.mouse_edit_state.kind).current_param;

            qint32 clock = quantize(s.mouse_edit_state.current_clock,
                                    m_client->quantizeClock());
            m_audio_note_preview = std::make_unique<NotePreview>(
                m_client->pxtn(), &m_client->moo()->params, unit_no, clock,
                std::list<EVERECORD>({e}), m_client->audioState()->bufferSize(),
                Settings::ChordPreview::get(), this);
          }
        }
      },
      false);
}

static void setVelInRange(const EVERECORD *&p, int32_t unit_no, qint32 unit_id,
                          const ParamEditInterval &interval,
                          std::list<Action::Primitive> &actions) {
  using namespace Action;
  while (p && p->prev && p->prev->clock >= interval.clock.start) p = p->prev;
  while (p && p->clock < interval.clock.start) p = p->next;
  for (; p && p->clock < interval.clock.end; p = p->next)
    if (p->kind == EVENTKIND_VELOCITY && p->unit_no == unit_no)
      actions.push_back(
          {EVENTKIND_VELOCITY, unit_id, p->clock, Add{interval.param}});
}

void ParamView::mouseReleaseEvent(QMouseEvent *event) {
  if (!(event->button() & (Qt::RightButton | Qt::LeftButton))) {
    event->ignore();
    return;
  }
  if (m_audio_note_preview) m_audio_note_preview = nullptr;
  if (!std::holds_alternative<MouseParamEdit>(
          m_client->editState().mouse_edit_state.kind))
    return;

  Interval clock_int(m_client->editState().mouse_edit_state.clock_int(
      m_client->quantizeClock()));

  EVENTKIND kind =
      paramOptions()[m_client->editState().current_param_kind_idx()].second;
  if (kind == EVENTKIND_PORTAMENT)
    clock_int = m_client->editState().mouse_edit_state.clock_int_short(
        m_client->quantizeClock());
  m_client->changeEditState(
      [&](EditState &s) {
        if (m_client->pxtn()->Unit_Num() > 0) {
          using namespace Action;
          std::list<Primitive> actions;
          switch (s.mouse_edit_state.type) {
            case MouseEditState::SetOn:
            case MouseEditState::DeleteOn:
            case MouseEditState::SetNote:
            case MouseEditState::DeleteNote:
              // Has gotten a bit convoluted, though this is because there are
              // like 3 different cases of adding something.
              actions.push_back({kind, s.m_current_unit_id, clock_int.start,
                                 Delete{clock_int.end}});
              // Velocity deletion is hard, so just interpret deletion as
              // setting.
              if (s.mouse_edit_state.type == MouseEditState::SetOn ||
                  s.mouse_edit_state.type == MouseEditState::SetNote ||
                  kind == EVENTKIND_VELOCITY) {
                if (kind == EVENTKIND_VOICENO) {
                  m_woice_menu->clear();
                  for (int i = 0; i < m_client->pxtn()->Woice_Num(); ++i) {
                    int32_t id = m_client->controller()->woiceIdMap().noToId(i);
                    QAction *action =
                        m_woice_menu->addAction(shift_jis_codec->toUnicode(
                            m_client->pxtn()->Woice_Get(i)->get_name_buf_jis(
                                nullptr)));
                    action->setData(id);
                  }
                  QAction *action = m_woice_menu->exec(event->globalPos());
                  if (m_audio_note_preview) m_audio_note_preview = nullptr;
                  if (action != nullptr) {
                    bool ok = true;
                    int woice_id = action->data().toInt(&ok);
                    if (!ok) {
                      qWarning() << "Invalid woice menu action woice ID";
                      return;
                    }
                    actions.push_back({kind, s.m_current_unit_id,
                                       clock_int.start, Add{woice_id}});
                  }
                } else if (!Evelist_Kind_IsTail(kind)) {
                  const std::list<ParamEditInterval> intervals = lineEdit(
                      s.mouse_edit_state, kind, m_client->quantizeClock());
                  if (kind == EVENTKIND_VELOCITY) {
                    std::optional<qint32> unit_no =
                        m_client->unitIdMap().idToNo(s.m_current_unit_id);
                    if (unit_no.has_value()) {
                      const EVERECORD *records =
                          m_client->pxtn()->evels->get_Records();
                      for (const ParamEditInterval &p : intervals)
                        setVelInRange(records, unit_no.value(),
                                      s.m_current_unit_id, p, actions);
                    }
                  } else
                    for (const ParamEditInterval &p : intervals)
                      actions.push_back({kind, s.m_current_unit_id,
                                         p.clock.start, Add{p.param}});
                } else {
                  actions.push_back({kind, s.m_current_unit_id, clock_int.start,
                                     Add{clock_int.length()}});
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
        updateStatePositions(s, event, kind, height());
      },
      false);
}

void ParamView::wheelEvent(QWheelEvent *event) {
  handleWheelEventWithModifier(event, m_client);
}

void ParamView::mouseMoveEvent(QMouseEvent *event) {
  // TODO: Change the note preview based off position.
  EVENTKIND current_kind =
      paramOptions()[m_client->editState().current_param_kind_idx()].second;
  if (!m_client->isFollowing())
    m_client->changeEditState(
        [&](auto &s) {
          updateStatePositions(s, event, current_kind, height());
        },
        true);
  event->ignore();
}
