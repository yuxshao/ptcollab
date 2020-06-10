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
#include "server/Client.h"

struct RemoteEditState {
  std::optional<EditState> state;
  QString user;
};

class KeyboardEditor : public QWidget {
  Q_OBJECT
 public:
  explicit KeyboardEditor(pxtnService *pxtn, QAudioOutput *audio_output,
                          Client *client, QScrollArea *parent = nullptr);
  void cycleCurrentUnit(int offset);
  void toggleShowAllUnits();
  void loadHistory(const QList<RemoteActionWithUid> &history);
  void setUid(qint64 uid);

 signals:
  void currentUnitChanged(int);
  void showAllChanged(bool);
  void onEdit();
  void editStateChanged(const EditState &e);
  void userListChanged(const QList<std::pair<qint64, QString>> &users);
  void quantXIndexChanged(int);

 public slots:
  void setQuantXIndex(int);
  void setQuantYIndex(int);
  void setCurrentUnit(int);
  void setShowAll(bool);
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
  Client *m_client;
  PxtoneActionSynchronizer *m_sync;
  int quantXIndex;
  int quantizeSelectionY;
  std::unordered_map<qint64, RemoteEditState> m_remote_edit_states;
  void refreshQuantSettings();
};

#endif  // KEYBOARDEDITOR_H
