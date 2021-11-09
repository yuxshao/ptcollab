#include "VolumeMeterWidget.h"

#include <QApplication>
#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>

#include "editor/Settings.h"
#include "editor/StyleEditor.h"

const double MIN_DB = -36;
const double MID_DB = -6;
const double HIGH_DB = -1;
const double MAX_DB = 3;

// TODO: make this colour dynamic

static QHash<QString, QColor> colorTable;

static QColor BGCOLOR;
static QColor BGCOLOR_SOFT;
static QColor BAR_COLOR;
static QColor BAR_MID_COLOR;
static QColor LABEL_COLOR;
static QColor TICK_COLOR;
static QColor BAR_HIGH_COLOR;

static QLinearGradient barGradient() {
  static QLinearGradient g = []() {
    QLinearGradient g(0, 0, 1, 0);
    g.setCoordinateMode(QGradient::ObjectMode);
    g.setColorAt(0, BAR_COLOR);
    g.setColorAt((MID_DB - MIN_DB) / (MAX_DB - MIN_DB), BAR_MID_COLOR);
    g.setColorAt((HIGH_DB - MIN_DB) / (MAX_DB - MIN_DB), BAR_HIGH_COLOR);
    return g;
  }();
  return g;
}

static const QColor &colAtDb(double db) {
  return (db > MID_DB ? (db > HIGH_DB ? BAR_HIGH_COLOR : BAR_MID_COLOR)
                      : BAR_COLOR);
}

VolumeMeterFrame::VolumeMeterFrame(const PxtoneClient *client, QWidget *parent)
    : QFrame(parent), m_client(client), m_animation(new Animation(this)) {
  setFrameStyle(QFrame::Panel | QFrame::Sunken);
  connect(m_animation, &Animation::nextFrame, this, [this]() { update(); });
  connect(client, &PxtoneClient::connected, this,
          &VolumeMeterFrame::resetPeaks);
}

void VolumeMeterFrame::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  p.fillRect(e->rect(), BGCOLOR);
  // int w_limit = dbToX(-3);

  const auto &levels = m_client->volumeLevels();
  while (levels.size() > m_peaks.size()) m_peaks.push_back(-INFINITY);
  for (uint i = 0; i < levels.size(); ++i) {
    int w = dbToX(levels[i].current_volume_dbfs());
    int y = (height() + 1) * i / levels.size();
    int h = (height() + 1) / levels.size() - 1;
    p.fillRect(QRect(0, y, width(), h), barGradient());

    p.fillRect(QRect(w, y, width() - w, h), BGCOLOR);

    for (int db = MIN_DB; db < MAX_DB; db += 3)
      p.fillRect(dbToX(db), y, 1, h, BGCOLOR_SOFT);
    p.fillRect(dbToX(-3), y, 1, h, BGCOLOR_SOFT);
    p.fillRect(dbToX(-3), y, 1, h, BGCOLOR_SOFT);

    double peak = levels[i].last_peak_dbfs();
    p.fillRect(QRect(dbToX(peak) - 1, y, 2, h), colAtDb(peak));
    if (peak > m_peaks[i]) m_peaks[i] = peak;
    if (m_peaks[i] > HIGH_DB)
      p.fillRect(QRect(dbToX(m_peaks[i]) - 1, y, 2, h), colAtDb(m_peaks[i]));
  }
  QFrame::paintEvent(e);
}

int VolumeMeterFrame::dbToX(double db) {
  return std::clamp<int>((db - MIN_DB) * width() / (MAX_DB - MIN_DB), 0,
                         width());
}

void VolumeMeterFrame::resetPeaks() {
  for (auto &p : m_peaks) p = -INFINITY;
}

QSize VolumeMeterFrame::minimumSizeHint() const { return QSize(0, 13); }

VolumeMeterLabels::VolumeMeterLabels(VolumeMeterFrame *frame, QWidget *parent)
    : QWidget(parent),
      m_frame(frame),
      m_show_text(Settings::ShowVolumeMeterLabels::get()) {}

void VolumeMeterLabels::refreshShowText() {
  m_show_text = Settings::ShowVolumeMeterLabels::get();
  updateGeometry();
}

constexpr int TICK_HEIGHT = 4;
constexpr int SMALL_TICK_HEIGHT = 2;

void VolumeMeterLabels::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  p.setFont(QFont("Sans serif", 6));
  p.setPen(LABEL_COLOR);

  p.drawText(QRect(0, 0, width() - 2, height() - TICK_HEIGHT),
             (Qt::AlignRight | Qt::AlignBottom), "dB");
  for (int db = MIN_DB + 6; db < MAX_DB; db += 6) {
    int x = m_frame->dbToX(db);
    p.drawText(QRect(x - 20, 0, 40, height() - TICK_HEIGHT),
               (Qt::AlignHCenter | Qt::AlignBottom), QString("%1").arg(db));
  }
  for (int db = MIN_DB; db < MAX_DB; db += 1) {
    int x = m_frame->dbToX(db);
    int h = (db % 6 == 0 ? TICK_HEIGHT : SMALL_TICK_HEIGHT);
    p.fillRect(x, height() - h, 1, h, TICK_COLOR);
  }
  QWidget::paintEvent(e);
}

QSize VolumeMeterLabels::minimumSizeHint() const {
  return QSize(0, m_show_text ? TICK_HEIGHT + 12 : TICK_HEIGHT);
}

VolumeMeterWidget::VolumeMeterWidget(VolumeMeterFrame *meter, QWidget *parent)
    : QWidget(parent),
      m_frame(meter),
      m_labels(new VolumeMeterLabels(meter, this)) {
  colorTable = StyleEditor::tryLoadMeterPalette();

  BGCOLOR = colorTable.find("Background").value();
  BGCOLOR_SOFT = colorTable.find("BackgroundSoft").value();
  BAR_COLOR = colorTable.find("Bar").value();
  BAR_MID_COLOR = colorTable.find("BarMid").value();
  LABEL_COLOR = colorTable.find("Label").value();
  TICK_COLOR = colorTable.find("Tick").value();
  BAR_HIGH_COLOR = colorTable.find("BarHigh").value();

  QVBoxLayout *layout = new QVBoxLayout;
  layout->setMargin(0);
  layout->setSpacing(0);
  setLayout(layout);
  layout->addWidget(m_labels);
  layout->addWidget(meter);

  meter->setSizePolicy(
      QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
  meter->setParent(this);
}

void VolumeMeterWidget::mousePressEvent(QMouseEvent *) {
  m_frame->resetPeaks();
}

void VolumeMeterWidget::mouseDoubleClickEvent(QMouseEvent *) {
  Settings::ShowVolumeMeterLabels::set(!Settings::ShowVolumeMeterLabels::get());
  refreshShowText();
}

void VolumeMeterWidget::refreshShowText() { m_labels->refreshShowText(); }
