#ifndef PXTONECLIENT_H
#define PXTONECLIENT_H

#include <QAudioOutput>
#include <QLabel>
#include <QObject>

#include "Clipboard.h"
#include "PxtoneController.h"
#include "network/Client.h"

struct RemoteEditState {
  std::optional<EditState> state;
  std::optional<quint64> last_ping;
  QString user;
};

struct UserListEntry {
  qint64 id;
  std::optional<quint64> last_ping;
  QString user;
};

// A class that hopefully all modifications to a project will through. It'll
// route the changes to the server and the responses to the controller, which
// will actually modify pxtnService.
class PxtoneClient : public QObject {
  Q_OBJECT

  PxtoneController *m_controller;
  Client *m_client;
  std::map<qint64, RemoteEditState> m_remote_edit_states;
  std::optional<qint64> m_following_user;
  mooState m_moo_state;
  EditState m_edit_state;
  QAudioOutput *m_audio;
  PxtoneIODevice *m_pxtn_device;
  std::optional<quint64> m_last_ping;
  QTimer *m_ping_timer;
  Clipboard *m_clipboard;

 signals:
  void userListChanged(const QList<UserListEntry> &users);
  void editStateChanged(const EditState &m_edit_state);
  void playStateChanged(bool playing);
  void followActivity(const EditState &r);
  void updatePing(std::optional<qint64> ping_length);
  void connected();

 public:
  PxtoneClient(pxtnService *pxtn, QLabel *client_status,

               QObject *parent = nullptr);
  void applyAction(const std::list<Action::Primitive> &);
  void sendAction(const ClientAction &);
  void removeCurrentUnit();
  void seekMoo(int64_t clock);
  void connectToServer(QString hostname, quint16 port, QString username);
  void disconnectFromServerSuppressSignal();
  void changeEditState(std::function<void(EditState &)>,
                       bool preserveFollow = false);
  void togglePlayState();
  void resetAndSuspendAudio();
  void setFollowing(std::optional<qint64> following);
  bool isFollowing();
  const pxtnService *pxtn() { return m_controller->pxtn(); }
  const EditState &editState() { return m_edit_state; }
  const mooState *moo() { return m_controller->moo(); }
  const QAudioOutput *audioState() { return m_audio; }

  const NoIdMap &unitIdMap() { return m_controller->unitIdMap(); }
  const std::map<qint64, RemoteEditState> &remoteEditStates() {
    return m_remote_edit_states;
  }
  qint64 uid() const;
  qint64 following_uid() const;
  qint32 quantizeClock(int idx);
  qint32 quantizeClock();
  qint32 quantizePitch(int idx);
  qint32 quantizePitch();

  void setCurrentUnitNo(int unit_no);
  void setCurrentWoiceNo(int woice_no);
  void setVolume(int volume);
  void deselect();
  const PxtoneController *controller() { return m_controller; }
  Clipboard *clipboard() { return m_clipboard; }

  void setBufferSize(double secs);
  bool isPlaying();

 private:
  void processRemoteAction(const ServerAction &a);
  void loadDescriptor(pxtnDescriptor &desc);
  void sendPlayState(bool from_action);
};

#endif  // PXTONECLIENT_H
