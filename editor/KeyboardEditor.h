#ifndef KEYBOARDEDITOR_H
#define KEYBOARDEDITOR_H

#include <QAudioOutput>
#include <QElapsedTimer>
#include <QScrollArea>
#include <QWidget>
#include <optional>

#include "Animation.h"
#include "DummySyncServer.h"
#include "PxtoneActionSynchronizer.h"
#include "PxtoneEditAction.h"
#include "pxtone/pxtnService.h"

struct Interval {
  int start;
  int end;

  bool contains(int x) const { return (start <= x && x < end); }

  int length() const { return end - start; }
};
struct MouseEditState {
  enum Type { Nothing, Seek, SetNote, SetOn, DeleteNote, DeleteOn };
  Type type;
  int start_clock;
  int start_pitch;
  int current_clock;
  int current_pitch;
  QAudioOutput *audio;
  Interval clock_int(int quantize);
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
                          QScrollArea *parent = nullptr);
  void cycleCurrentUnit(int offset);
  void toggleShowAllUnits();

 signals:
  void currentUnitChanged(int);
  void showAllChanged(bool);
  void onEdit();

 public slots:
  void setQuantX(int);
  void setQuantY(int);
  void setCurrentUnit(int);
  void undo();
  void redo();

 private:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void refreshSize();
  QSize sizeHint() const override;
  QAudioOutput *make_audio(int pitch);
  Scale scale;
  pxtnService *m_pxtn;
  QElapsedTimer *m_timer;
  int m_current_unit;
  bool m_show_all_units;
  int painted;
  MouseEditState m_mouse_edit_state;
  QAudioOutput *m_audio_output;
  Animation *m_anim;
  int m_quantize_clock;
  int m_quantize_pitch;
  // TODO: How to represent it when undos are interleaved?
  std::vector<std::vector<Action>> actionHistory;
  int actionHistoryPosition;
  // TODO: Think about how this might screw up upon file load.
  DummySyncServer server;
};

#endif  // KEYBOARDEDITOR_H
