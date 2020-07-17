#ifndef PXTONECLIENT_H
#define PXTONECLIENT_H

#include <QAudioOutput>
#include <QLabel>
#include <QObject>

#include "PxtoneController.h"
#include "UnitListModel.h"
#include "server/Client.h"

struct RemoteEditState {
  std::optional<EditState> state;
  QString user;
};

// A class that hopefully all modifications to a project will through. It'll
// route the changes to the server and the responses to the controller, which
// will actually modify pxtnService.
class PxtoneClient : public QObject {
  Q_OBJECT

  PxtoneController *m_controller;
  Client *m_client;
  std::unordered_map<qint64, RemoteEditState> m_remote_edit_states;
  mooState m_moo_state;
  UnitListModel *m_units;
  EditState m_edit_state;
  QAudioOutput *m_audio;
  PxtoneIODevice *m_pxtn_device;

 signals:
  void measureNumChanged();
  void userListChanged(const QList<std::pair<qint64, QString>> &users);
  void tempoBeatChanged();
  void woicesChanged();
  void editStateChanged(const EditState &m_edit_state);
  void currentUnitNoChanged(int unit_no);
  void playStateChanged(bool playing);
  void seeked(int clock);
  void edited();
  void connected();

 public:
  PxtoneClient(pxtnService *pxtn, QLabel *client_status, UnitListModel *units,
               QObject *parent = nullptr);
  void applyAction(const std::list<Action::Primitive> &);
  void sendAction(const ClientAction &);
  void removeCurrentUnit();
  void seekMoo(int64_t clock);
  void connectToServer(QString hostname, quint16 port, QString username);
  void changeEditState(std::function<void(EditState &)>);
  void togglePlayState();
  void resetAndSuspendAudio();
  const pxtnService *pxtn() { return m_controller->pxtn(); }
  const EditState &editState() { return m_edit_state; }
  const mooState *moo() { return m_controller->moo(); }
  const QAudioOutput *audioState() { return m_audio; }

  const UnitIdMap &unitIdMap() { return m_controller->unitIdMap(); }
  const std::unordered_map<qint64, RemoteEditState> &remoteEditStates() {
    return m_remote_edit_states;
  }
  qint64 uid() { return m_controller->uid(); }

  void setCurrentUnitNo(int unit_no);

 private:
  void processRemoteAction(const ServerAction &a);
  void loadDescriptor(pxtnDescriptor &desc);
};

#endif  // PXTONECLIENT_H
