#ifndef KEYBOARDEDITOR_H
#define KEYBOARDEDITOR_H

#include <QWidget>
#include "pxtone/pxtnService.h"
#include "Animation.h"
#include <QAudioOutput>
#include <QElapsedTimer>

class KeyboardEditor : public QWidget
{
    Q_OBJECT
public:
    explicit KeyboardEditor(pxtnService *pxtn,  QAudioOutput *audio_output, QWidget *parent = nullptr);

signals:

private:
    void mousePressEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *event) override;
    pxtnService *m_pxtn;
    QElapsedTimer *m_timer;
    int painted;
    QAudioOutput *m_audio_output;
    Animation *m_anim;
};

#endif // KEYBOARDEDITOR_H
