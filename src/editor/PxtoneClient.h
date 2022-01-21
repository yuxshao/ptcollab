#ifndef PXTONECLIENT_H
#define PXTONECLIENT_H

#include <QAudioOutput>
#include <QLabel>
#include <QObject>

#include "Clipboard.h"
#include "ConnectionStatusLabel.h"
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
  qint32 m_last_seek;
  Clipboard *m_clipboard;

 signals:
  void editStateChanged(const EditState &m_edit_state);
  void playStateChanged(bool playing);
  void followActivity(const EditState &r);
  void updatePing(std::optional<qint64> ping_length);
  void connected();

  void beginAddUser(int index);
  void endAddUser();
  void beginRemoveUser(int index);
  void endRemoveUser();
  void beginUserListRefresh();
  void endUserListRefresh();

 public:
  PxtoneClient(pxtnService *pxtn, ConnectionStatusLabel *connection_status,

               QObject *parent = nullptr);
  void applyAction(const std::list<Action::Primitive> &);
  void sendAction(const ClientAction &);
  void removeCurrentUnit();
  void seekMoo(int64_t clock);
  void connectToServer(QString hostname, quint16 port, QString username);
  void connectToLocalServer(BroadcastServer *server, QString username);
  void disconnectFromServerSuppressSignal();
  void changeEditState(std::function<void(EditState &)>, bool preserveFollow);
  void togglePlayState();
  void resetAndSuspendAudio();
  void setFollowing(std::optional<qint64> following);
  void jumpToUser(qint64 user_id);
  bool isFollowing();
  std::set<int> selectedUnitNos();
  const pxtnService *pxtn() const { return m_controller->pxtn(); }
  const EditState &editState() const { return m_edit_state; }
  const mooState *moo() const { return m_controller->moo(); }
  const QAudioOutput *audioState() const { return m_audio; }

  const NoIdMap &unitIdMap() const { return m_controller->unitIdMap(); }
  const std::map<qint64, RemoteEditState> &remoteEditStates() {
    return m_remote_edit_states;
  }
  qint64 uid() const;
  qint64 following_uid() const;
  qint32 quantizeClock(int idx);
  qint32 quantizeClock();
  qint32 lastSeek() const;

  void setCurrentUnitNo(int unit_no, bool preserveFollow);
  void setCurrentWoiceNo(int woice_no, bool preserveFollow);
  void setVolume(int volume);
  void deselect(bool preserveFollow);
  void selectAllUnits(bool select);
  const PxtoneController *controller() { return m_controller; }
  Clipboard *clipboard() { return m_clipboard; }

  void setBufferSize(double secs);
  bool isPlaying();

  void setUnitPlayed(int unit_no, bool played);
  void setUnitVisible(int unit_no, bool visible);
  void setUnitOperated(int unit_no, bool operated);
  void cycleSolo(int unit_no);
  void removeUnusedUnitsAndWoices();
  const std::vector<InterpolatedVolumeMeter> &volumeLevels() const;

 private:
  void processRemoteAction(const ServerAction &a);
  void loadDescriptor(pxtnDescriptor &desc);
  void sendPlayState(bool from_action);
};

#endif  // PXTONECLIENT_H
