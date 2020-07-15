#ifndef KEYBOARDEDITOR_H
#define KEYBOARDEDITOR_H

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
#include "PxtoneController.h"
#include "UnitListModel.h"
#include "pxtone/pxtnService.h"
#include "server/Client.h"

enum struct Direction { UP, DOWN };

struct RemoteEditState {
  std::optional<EditState> state;
  QString user;
};

class KeyboardEditor : public QWidget {
  Q_OBJECT
 public:
  explicit KeyboardEditor(pxtnService *pxtn, mooState *moo_state,
                          QAudioOutput *audio_output, Client *client,
                          UnitListModel *units, QScrollArea *parent = nullptr);
  void cycleCurrentUnit(int offset);
  void loadHistory(QList<ServerAction> &history);
  void setUid(qint64 uid);
  void resetUnitIdMap();
  const EditState &edit_state() { return m_edit_state; };

 signals:
  void currentUnitNoChanged(int);
  void unitsChanged();
  void woicesChanged();
  void tempoBeatChanged();
  void onEdit();
  void editStateChanged();
  void userListChanged(const QList<std::pair<qint64, QString>> &users);
  void quantXIndexChanged(int);

 public slots:
  void setQuantXIndex(int);
  void setQuantYIndex(int);
  void setCurrentUnitNo(int);
  void removeCurrentUnit();
  void clearRemoteEditStates();
  void processRemoteAction(const ServerAction &a);
  void toggleTestActivity();
  void seekPosition(int clock);
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
  mooState *m_moo_state;
  QElapsedTimer *m_timer;
  int painted;
  EditState m_edit_state;
  QAudioOutput *m_audio_output;
  std::unique_ptr<NotePreview> m_audio_note_preview;
  Animation *m_anim;
  Client *m_client;
  PxtoneController *m_sync;
  int quantXIndex;
  int quantizeSelectionY;

  // A bunch of things to give the illusion of a smooth playhead when there's a
  // buffer.
  int m_last_clock, m_this_seek;
  bool m_this_seek_caught_up;
  QElapsedTimer timeSinceLastClock;

  bool m_test_activity;
  std::unordered_map<qint64, RemoteEditState> m_remote_edit_states;
  Clipboard m_clipboard;
  UnitListModel *m_units;
};

#endif  // KEYBOARDEDITOR_H
