#ifndef PXTONECONTROLLER_H
#define PXTONECONTROLLER_H
#include <QObject>
#include <QTextCodec>
#include <list>

#include "audio/PxtoneIODevice.h"
#include "protocol/PxtoneEditAction.h"
#include "protocol/RemoteAction.h"

// Okay, I give up on eager undo. It's just way too hard to roll back an undo
// from the local branch.

struct LoggedAction {
  enum UndoState : qint8 { DONE, UNDONE, GONE };
  UndoState state;
  qint64 uid;
  qint64 idx;
  // TODO: Figure out where to call sth undo vs. reverse
  std::list<Action::Primitive> reverse;
  LoggedAction(qint64 uid, qint64 idx,
               const std::list<Action::Primitive> &reverse)
      : state(DONE), uid(uid), idx(idx), reverse(reverse) {}
};

class PxtoneController : public QObject {
  Q_OBJECT
 public:
  PxtoneController(int uid, pxtnService *pxtn, mooState *moo_state,
                   QObject *parent);

  // rename to applyAndGetEditAction and applyEditAction
  EditAction applyLocalAction(const std::list<Action::Primitive> &action);
  void setUid(qint64 uid);
  qint64 uid();
  const NoIdMap &unitIdMap() const { return m_unit_id_map; }
  const NoIdMap &woiceIdMap() const { return m_woice_id_map; }
  bool loadDescriptor(pxtnDescriptor &desc);
  bool applyAddUnit(const AddUnit &a, qint64 uid);
  bool applyAddWoice(const AddWoice &a, qint64 uid);
  bool applyRemoveWoice(const RemoveWoice &a, qint64 uid);
  bool applyChangeWoice(const ChangeWoice &a, qint64 uid);
  bool applyWoiceSet(const Woice::Set &a, qint64 uid);
  bool applyTempoChange(const TempoChange &a, qint64 uid);
  bool applyBeatChange(const BeatChange &a, qint64 uid);
  void seekMoo(int64_t clock);
  void refreshMoo();
  const mooState *moo() { return m_moo_state; }
  const pxtnService *pxtn() { return m_pxtn; };
  void setVolume(int volume);
  void setSongTitle(const QString &);
  void setSongComment(const QString &);

  void setUnitPlayed(int unit_no, bool played);
  void setUnitVisible(int unit_no, bool visible);
  void setUnitOperated(int unit_no, bool operated);
  void cycleSolo(int unit_no);
  bool render_exn(
      QIODevice *file, double secs, double fadeout, double volume,
      std::optional<size_t> solo_unit,
      std::function<bool(double progress)> should_continue = [](double) {
        return true;
      }) const;

 public slots:
  // Maybe these types could be grouped.
  void applyRemoteAction(const EditAction &a, qint64 uid);
  void applyUndoRedo(const UndoRedo &r, qint64 uid);
  void applyRemoveUnit(const RemoveUnit &a, qint64 uid);
  void applyMoveUnit(const MoveUnit &a, qint64 uid);
  void applySetRepeatMeas(const SetRepeatMeas &a, qint64 uid);
  void applySetLastMeas(const SetLastMeas &a, qint64 uid);
  void applySetUnitName(const SetUnitName &a, qint64 uid);
  void applySetOverdrive(const Overdrive::Set &a, qint64 uid);
  void applyAddOverdrive(const Overdrive::Add &a, qint64 uid);
  void applyRemoveOverdrive(const Overdrive::Remove &a, qint64 uid);
  void applySetDelay(const DelayEffect::Set &a, qint64 uid);

 signals:
  void measureNumChanged();
  void tempoBeatChanged();
  void playedToggled(int unit_no);
  void operatedToggled(int unit_no, bool operated);
  void soloToggled();
  void newSong();
  void edited();

  void seeked(qint32 clock);

  void beginAddUnit();
  void endAddUnit();
  void unitNameEdited(int index);
  void beginRemoveUnit(int index);
  void endRemoveUnit();

  void beginAddWoice();
  void endAddWoice();
  void woiceEdited(int index);
  void beginRemoveWoice(int index);
  void endRemoveWoice();

  void beginAddOverdrive();
  void endAddOverdrive();
  void overdriveChanged(int ovdrv_no);
  void beginRemoveOverdrive(int ovdrv_no);
  void endRemoveOverdrive();

  void delayChanged(int delay_no);

  void beginRefresh();
  void endRefresh();

  void beginMoveUnit(int unit_no, bool up);
  void endMoveUnit();

 private:
  qint64 m_uid;
  pxtnService *m_pxtn;
  mooState *m_moo_state;
  PxtoneIODevice *m_moo_io_device;

  std::vector<LoggedAction> m_log;
  std::list<std::list<Action::Primitive>> m_uncommitted;
  NoIdMap m_unit_id_map, m_woice_id_map;
  int m_remote_index;
};

const extern QTextCodec *shift_jis_codec;

#endif  // PXTONECONTROLLER_H
