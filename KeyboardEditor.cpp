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
    //connect(m_audio_output, SIGNAL(notify()), SLOT(update()));
}

struct KeyBlock {
    int pitch;
    int start;
    int end;
};

int clockPerPx = 10;
int pitchPerPx = 32;
int pitchOffset = 38400;
int height = 5;
static qreal pitchToY(qreal pitch) {
    return (pitchOffset - pitch) / pitchPerPx;
}
static qreal pitchOfY(qreal y) {
    return pitchOffset - y * pitchPerPx;
}

static void paintBlock(const KeyBlock &block, QPainter &painter, const QBrush &brush) {

    painter.fillRect(block.start/clockPerPx, pitchToY(block.pitch),
                     (block.end - block.start)/clockPerPx, height, brush);
}

static void paintPlayhead(QPainter &painter, QBrush &brush, int clock) {
    painter.fillRect(clock/clockPerPx, 0, 1, 10000, brush);
}

static bool blockIsActive(const KeyBlock &block, int clock) {
    return (clock >= block.start && clock < block.end);
}


static qreal iFps;
void KeyboardEditor::paintEvent(QPaintEvent *) {
    ++painted;
    //if (painted > 10) return;
    QPainter painter(this);
    {
        int interval = 20;
        if (!(painted % interval)) {
            qint64 elapsed = m_timer->nsecsElapsed();
            m_timer->restart();
            iFps = 1E9/elapsed*interval;
        }
        painter.drawText(rect(), QString("%1 FPS").arg(iFps, 0, 'f', 0));
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
    // TODO: usecs is choppy - it's an upper bound that gets worse with buffer size incrase.
    // for longer songs though and lower end comps we probably do want a bigger buffer.
    // The formula fixes the upper bound issue, but perhaps we can do some smoothing with a linear thing too.
    //int bytes_per_second = 4 /* bytes in sample */ * 44100 /* samples per second */;
    //long usecs = m_audio_output->processedUSecs() - long(m_audio_output->bufferSize()) * 10E5 / bytes_per_second;

    long usecs = m_audio_output->processedUSecs();
    int clock = usecs * m_pxtn->master->get_beat_tempo() * m_pxtn->master->get_beat_clock() / 60 / 1000000;
    //clock = m_pxtn->moo_get_now_clock();
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
            // TODO: This rendering is weird
            blocks[i].end = std::min(e->clock, blocks[i].end);
            paintBlock(blocks[i], painter, blockIsActive(blocks[i], clock) ? activeBrushes[i] : brushes[i]);
            blocks[i].pitch = e->value;
            blocks[i].start = e->clock;
            break;
        default: break;
        }
    }

    if (m_mouse_edit_state != nullptr) {
        KeyBlock b{m_mouse_edit_state->start_pitch, m_mouse_edit_state->start_clock, m_mouse_edit_state->current_clock};
        paintBlock(b, painter, brushes[0]);
    }

    QBrush b(QColor::fromRgb(255,255,255));
    // clock = us * 1s/10^6us * 1m/60s * tempo beats/m * beat_clock clock/beats
    paintPlayhead(painter, b, clock);
    //painter.fillRect(-3, -3, 100, 100, brush);
    painter.setFont(QFont("Arial", 30));
    painter.drawText(rect(), Qt::AlignCenter, "Qst");

}

void KeyboardEditor::mousePressEvent ( QMouseEvent * event ) {
    int clock = event->localPos().x()*clockPerPx;
    int pitch = int(round(pitchOfY(event->localPos().y())));
    MouseEditState::Type type;
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->button() == Qt::RightButton)
            type = MouseEditState::Type::DeleteNote;
        else type = MouseEditState::Type::SetNote;
    }
    else {
        if (event->button() == Qt::RightButton)
            type = MouseEditState::Type::DeleteOn;
        else type = MouseEditState::Type::SetOn;
    }

    m_mouse_edit_state.reset(new MouseEditState{type, clock, pitch, clock, pitch});
}

void KeyboardEditor::mouseMoveEvent(QMouseEvent *event) {
    if (m_mouse_edit_state == nullptr) return;
    m_mouse_edit_state->current_pitch = int(round(pitchOfY(event->localPos().y())));
    m_mouse_edit_state->current_clock = event->localPos().x()*clockPerPx;
}

void KeyboardEditor::mouseReleaseEvent(QMouseEvent *event) {
    if (m_mouse_edit_state == nullptr) return;

    int start_clock = m_mouse_edit_state->start_clock;
    int end_clock = event->localPos().x()*clockPerPx;
    int start_pitch = m_mouse_edit_state->start_pitch;
    int end_pitch = int(round(pitchOfY(event->localPos().y())));
    if (end_pitch < start_pitch) std::swap(end_pitch, start_pitch);
    qDebug() << m_mouse_edit_state->type;

    switch (m_mouse_edit_state->type) {
    case MouseEditState::SetOn:
        m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_ON);
        m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_VELOCITY);
        m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_KEY);
        m_pxtn->evels->Record_Add_i(start_clock, 0, EVENTKIND_ON, end_clock-start_clock);
        m_pxtn->evels->Record_Add_i(start_clock, 0, EVENTKIND_VELOCITY, EVENTDEFAULT_VELOCITY);
        m_pxtn->evels->Record_Add_i(start_clock, 0, EVENTKIND_KEY, end_clock-start_clock);
        break;
    case MouseEditState::DeleteOn:
        m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_ON);
        m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_VELOCITY);
        break;
    case MouseEditState::SetNote:
        m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_KEY);
        m_pxtn->evels->Record_Add_i(start_clock, 0, EVENTKIND_KEY, start_pitch);
        break;
    case MouseEditState::DeleteNote:
        m_pxtn->evels->Record_Delete(start_clock, end_clock, 0, EVENTKIND_KEY);
        break;
    }
    m_mouse_edit_state.reset();
}

