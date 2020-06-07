#ifndef KEYBOARDEDITOR_H
#define KEYBOARDEDITOR_H

#include <QAudioOutput>
#include <QElapsedTimer>
#include <QScrollArea>
#include <QWidget>
#include <optional>

#include "Animation.h"
#include "DummySyncServer.h"
#include "EditState.h"
#include "PxtoneActionSynchronizer.h"
#include "pxtone/pxtnService.h"
#include "server/ActionClient.h"

class KeyboardEditor : public QWidget {
  Q_OBJECT
 public:
  explicit KeyboardEditor(pxtnService *pxtn, QAudioOutput *audio_output,
                          ActionClient *client, QScrollArea *parent = nullptr);
  void cycleCurrentUnit(int offset);
  void toggleShowAllUnits();
  void loadHistory(const QList<RemoteActionWithUid> &history);
  void setUid(qint64 uid);

 signals:
  void currentUnitChanged(int);
  void showAllChanged(bool);
  void onEdit();

 public slots:
  void setQuantX(int);
  void setQuantY(int);
  void setCurrentUnit(int);
  void setRemoteEditState(qint32 uid, const EditState &state);
  void clearRemoteEditState(qint32 uid);
  void clearRemoteEditStates();
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
  pxtnService *m_pxtn;
  QElapsedTimer *m_timer;
  bool m_show_all_units;
  int painted;
  EditState m_edit_state;
  QAudioOutput *m_audio_output;
  QAudioOutput *m_audio_note_preview;
  Animation *m_anim;
  ActionClient *m_client;
  PxtoneActionSynchronizer m_sync;
  std::unordered_map<int, EditState> m_remote_edit_states;
};

#endif  // KEYBOARDEDITOR_H
