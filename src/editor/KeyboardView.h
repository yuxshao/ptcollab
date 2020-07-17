#ifndef KEYBOARDVIEW_H
#define KEYBOARDVIEW_H

#include <QAudioOutput>
#include <QElapsedTimer>
#include <QScrollArea>
#include <QWidget>
#include <optional>

#include "Animation.h"
#include "Clipboard.h"
#include "DummySyncServer.h"
#include "EditState.h"
#include "NotePreview.h"
#include "PxtoneClient.h"
#include "UnitListModel.h"
#include "pxtone/pxtnService.h"

enum struct Direction { UP, DOWN };

struct LocalEditState {
  qint32 m_quantize_clock, m_quantize_pitch;
  LocalEditState(const pxtnService *pxtn, const EditState &s) {
    update(pxtn, s);
  };
  void update(const pxtnService *pxtn, const EditState &s);
};

class KeyboardView : public QWidget {
  Q_OBJECT
 public:
  explicit KeyboardView(PxtoneClient *client, QScrollArea *parent = nullptr);
  void cycleCurrentUnit(int offset);

 public slots:
  void toggleTestActivity();
  void selectAll();
  void deselect();
  void transposeSelection(Direction dir, bool wide, bool shift);
  void cutSelection();
  void copySelection();
  void clearSelection();
  void paste();

 private:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void refreshSize();
  void refreshQuantSettings();
  QSize sizeHint() const override;
  std::set<int> selectedUnitNos();
  const pxtnService *m_pxtn;
  QElapsedTimer *m_timer;
  int painted;
  LocalEditState m_edit_state;
  std::unique_ptr<NotePreview> m_audio_note_preview;
  Animation *m_anim;
  PxtoneClient *m_client;

  // A bunch of things to give the illusion of a smooth playhead when there's a
  // buffer.
  int m_last_clock, m_this_seek;
  bool m_this_seek_caught_up;
  QElapsedTimer timeSinceLastClock;

  bool m_test_activity;
  Clipboard m_clipboard;
};

#endif  // KEYBOARDVIEW_H
