#include "KeyboardEditor.h"
#include <QPainter>
#include <QDebug>
#include <QTime>
#include <QPaintEvent>

KeyboardEditor::KeyboardEditor(pxtnService *pxtn, QAudioOutput *audio_output, QWidget *parent) :
    QWidget(parent), m_pxtn(pxtn), painted(0), m_timer(new QElapsedTimer), m_audio_output(audio_output), m_anim(new Animation(this))
{
    m_audio_output->setNotifyInterval(10);
    qDebug() << m_audio_output->notifyInterval();
    m_anim->setDuration(100);
    m_anim->setStartValue(0);
    m_anim->setEndValue(360);
    m_anim->setEasingCurve(QEasingCurve::Linear);
    m_anim->setLoopCount(-1);
    m_anim->start();

    connect(m_anim, SIGNAL(valueChanged(QVariant)), SLOT(update()));
}

struct KeyBlock {
    int pitch;
    int start;
    int end;
};
int xscale = 20;
int yscale = 128;
int height = 3;
static void paintBlock(const KeyBlock &block, QPainter &painter, const QBrush &brush) {

    painter.fillRect(block.start/xscale, 300-block.pitch/yscale, (block.end - block.start)/xscale, height, brush);
}

static void paintPlayhead(QPainter &painter, QBrush &brush, int clock) {
    painter.fillRect(clock/xscale, 0, 1, 10000, brush);
}

static bool blockIsActive(const KeyBlock &block, int clock) {
    return (clock >= block.start && clock < block.end);
}


static qreal m_elapsed = 1000000000;
static qreal iFps, m_fps;
void KeyboardEditor::paintEvent(QPaintEvent *) {
    ++painted;
    //if (painted > 10) return;
    QPainter painter(this);
    {
        int interval = 60;
        if (!(painted % interval)) {
            const qreal t = 0.4;
            qint64 elapsed = m_timer->nsecsElapsed();
            m_timer->restart();
            m_elapsed = (1.0-t)*m_elapsed + t*elapsed;
            iFps = 1E9/elapsed*interval;
            m_fps = 1E9/m_elapsed*interval;
        }
        painter.drawText(rect(), QString("%1,%2 FPS %3").arg(m_fps, 0, 'f', 0).arg(iFps, 0, 'f', 0).arg(m_elapsed, 0, 'i', 0));
    }
    std::vector<KeyBlock> blocks;
    std::vector<QBrush> brushes;
    std::vector<QBrush> activeBrushes;
    for (int i = 0; i < m_pxtn->Unit_Num(); ++i) {
        blocks.push_back(KeyBlock{EVENTDEFAULT_KEY, 0, 0});
        brushes.push_back(QBrush(QColor::fromHsl((360*i*3/7) % 360, 255, 128)));
        activeBrushes.push_back(QBrush(QColor::fromHsl((360*i*3/7) % 360, 255, 240)));
    }
    painter.setPen(Qt::blue);
    int clock = m_audio_output->processedUSecs() * m_pxtn->master->get_beat_tempo() * m_pxtn->master->get_beat_clock() / 60 / 1000000;
    int i = 0;
    for (const EVERECORD* e = m_pxtn->evels->get_Records(); e != nullptr; e = e->next) {
        int i = e->unit_no;
        if (i >= brushes.size()) continue;
        switch (e->kind) {
        case EVENTKIND_ON:
            if (e->clock < blocks[i].end) blocks[i].end = e->clock;
            // qDebug() << block.pitch << " " << block.start << " " << block.end;
            paintBlock(blocks[i], painter, blockIsActive(blocks[i], clock) ? activeBrushes[i] : brushes[i]);
            blocks[i].start = e->clock; blocks[i].end = e->clock + e->value;
            break;
        case EVENTKIND_KEY:
            blocks[i].end = e->clock;
            paintBlock(blocks[i], painter, blockIsActive(blocks[i], clock) ? activeBrushes[i] : brushes[i]);
            blocks[i].pitch = e->value;
            blocks[i].start = e->clock;
            break;
        default: break;
        }
    }

    QBrush b(QColor::fromRgb(255,255,255));
    // clock = us * 1s/10^6us * 1m/60s * tempo beats/m * beat_clock clock/beats
    paintPlayhead(painter, b, clock);
    //painter.fillRect(-3, -3, 100, 100, brush);
    painter.setFont(QFont("Arial", 30));
    painter.drawText(rect(), Qt::AlignCenter, "Qst");

}
