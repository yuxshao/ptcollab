#ifndef KEYBOARDEDITOR_H
#define KEYBOARDEDITOR_H

#include <QAudioOutput>
#include <QElapsedTimer>
#include <QWidget>
#include <optional>

#include "Animation.h"
#include "pxtone/pxtnService.h"

struct MouseEditState {
  enum Type { Nothing, Seek, SetNote, SetOn, DeleteNote, DeleteOn };
  Type type;
  int start_clock;
  int start_pitch;
  int current_clock;
  int current_pitch;
};

constexpr int PITCH_PER_KEY = 256;
constexpr int EVENTMAX_KEY = 135 * PITCH_PER_KEY;
constexpr int EVENTMIN_KEY = 46 * PITCH_PER_KEY;
struct Scale {
  double clockPerPx;
  double pitchPerPx;
  int pitchOffset;
  int noteHeight;

  Scale()
      : clockPerPx(10),
        pitchPerPx(32),
        pitchOffset(EVENTMAX_KEY),
        noteHeight(5) {}
  qreal pitchToY(qreal pitch) const {
    return (pitchOffset - pitch) / pitchPerPx;
  }
  qreal pitchOfY(qreal y) const { return pitchOffset - y * pitchPerPx; }
};

class KeyboardEditor : public QWidget {
  Q_OBJECT
 public:
  explicit KeyboardEditor(pxtnService *pxtn, QAudioOutput *audio_output,
                          QWidget *parent = nullptr);

 signals:

 private:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void refreshSize();
  QSize sizeHint() const override;
  Scale scale;
  pxtnService *m_pxtn;
  QElapsedTimer *m_timer;
  int painted;
  MouseEditState m_mouse_edit_state;
  QAudioOutput *m_audio_output;
  Animation *m_anim;
};

#endif  // KEYBOARDEDITOR_H
