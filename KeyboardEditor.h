#ifndef KEYBOARDEDITOR_H
#define KEYBOARDEDITOR_H

#include <QWidget>
#include "pxtone/pxtnService.h"
#include "Animation.h"
#include <QAudioOutput>
#include <QElapsedTimer>
#include <optional>

struct MouseEditState {
    enum Type {
        SetNote,
        SetOn,
        DeleteNote,
        DeleteOn
    };
    Type type;
    int start_clock;
    int start_pitch;
    int current_clock;
    int current_pitch;
};


class KeyboardEditor : public QWidget
{
    Q_OBJECT
public:
    explicit KeyboardEditor(pxtnService *pxtn,  QAudioOutput *audio_output, QWidget *parent = nullptr);

signals:

private:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    pxtnService *m_pxtn;
    QElapsedTimer *m_timer;
    int painted;
    std::unique_ptr<MouseEditState> m_mouse_edit_state;
    QAudioOutput *m_audio_output;
    Animation *m_anim;
};

#endif // KEYBOARDEDITOR_H
