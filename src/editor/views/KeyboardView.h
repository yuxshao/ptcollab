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
 signals:
  void ensureVisibleX(int x, bool strict);
  void fpsUpdated(qreal fps);

 public slots:
  void toggleTestActivity();
  void selectAll(bool preserveFollow);
  void transposeSelection(Direction dir, bool wide, bool shift);
  void cutSelection();
  void copySelection();
  void clearSelection();
  void quantizeSelectionX();
  void quantizeSelectionY();
  void paste(bool preserveFollow);
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

  bool m_test_activity;
};

#endif  // KEYBOARDVIEW_H
