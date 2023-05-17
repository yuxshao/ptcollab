#ifndef KEYBOARDVIEW_H
#define KEYBOARDVIEW_H

#include <QAudioOutput>
#include <QElapsedTimer>
#include <QScrollArea>
#include <QWidget>
#include <optional>

#include "../EditState.h"
#include "Animation.h"
#include "MooClock.h"
#include "editor/PxtoneClient.h"
#include "editor/audio/NotePreview.h"
#include "pxtone/pxtnService.h"

enum struct Direction : qint8 { UP, DOWN };

struct LocalEditState {
  qint32 m_quantize_clock, m_quantize_pitch;
  Scale scale;
  LocalEditState(const pxtnService *pxtn, const EditState &s) {
    update(pxtn, s);
  };
  void update(const pxtnService *pxtn, const EditState &s);
};

class KeyboardView : public QWidget {
  Q_OBJECT
 public:
  explicit KeyboardView(PxtoneClient *client, MooClock *moo_clock,
                        QScrollArea *parent = nullptr);
  void cycleCurrentUnit(int offset, bool selectedOnly);
  void setCurrentUnitNo(int unit_no, bool preserve_follow);

  void ensurePlayheadFollowed();
  void setFocusedUnit(std::optional<int> unit_no);
  void setSelectUnitEnabled(bool);
  std::map<int, int> &currentMidiNotes();

 signals:
  void ensureVisibleX(int x, bool strict);
  void fpsUpdated(qreal fps);
  void hoverUnitNoChanged(std::optional<int>);
  void setScrollOnClick(bool);

 public slots:
  void toggleTestActivity();
  void selectAll(bool preserveFollow);
  void transposeSelection(Direction dir, bool wide, bool shift);
  void cutSelection();
  void copySelection();
  void clearSelection();
  void quantizeSelectionX();
  void quantizeSelectionY();
  void paste(bool useSelectionEnd, bool preserveFollow);
  void toggleDark();

 private:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void moveEvent(QMoveEvent *event) override;
  void refreshQuantSettings();
  QSize sizeHint() const override;
  std::set<int> selectedUnitNos();
  void setHoveredUnitNo(std::optional<int>);
  void updateHoverSelect(QMouseEvent *event);
  void updateStatePositions(EditState &, const QMouseEvent *, int leftPos);
  QScrollArea *m_scrollarea;
  const pxtnService *m_pxtn;
  QElapsedTimer *m_timer;
  int painted;
  bool m_dark;
  LocalEditState m_edit_state;
  std::unique_ptr<NotePreview> m_audio_note_preview;
  Animation *m_anim;
  PxtoneClient *m_client;
  MooClock *m_moo_clock;
  std::optional<int> m_focused_unit_no;
  std::optional<int> m_hovered_unit_no;
  std::map<int, int> m_midi_notes;
  // m_select_unit_enabled should prob be folded into edit state. Right now it's
  // just a sore thumb of local state
  bool m_select_unit_enabled;
  int m_last_left_kb_pitch;

  bool m_test_activity;
};

#endif  // KEYBOARDVIEW_H
