#ifndef SIDEMENU_H
#define SIDEMENU_H

#include <QFileDialog>
#include <QInputDialog>
#include <QStringListModel>
#include <QWidget>

#include "DelayEffectModel.h"
#include "OverdriveEffectModel.h"
#include "SelectWoiceDialog.h"
#include "UnitListModel.h"
#include "UserListModel.h"
#include "VolumeMeterWidget.h"
#include "WoiceListModel.h"
#include "editor/NewWoiceDialog.h"

namespace Ui {
class SideMenu;
}

// TODO: Add a PxtoneSideMenu that takes a client and routes the signals /
// slots?
class SideMenu : public QWidget {
  Q_OBJECT

 public:
  explicit SideMenu(UnitListModel *units, WoiceListModel *woices,
                    UserListModel *users, SelectWoiceDialog *add_unit_dialog,
                    DelayEffectModel *delays, OverdriveEffectModel *ovdrvs,
                    NewWoiceDialog *new_woice_dialog,
                    NewWoiceDialog *change_woice_dialog,
                    VolumeMeterFrame *volume_meter_widget,
                    QWidget *parent = nullptr);
  void setEditWidgetsEnabled(bool);
  void setTab(int index);

  ~SideMenu();

 signals:
  void quantXIndexActivated(int);
  void quantYIndexActivated(int);
  void currentUnitChanged(int);
  void unitClicked(int);
  void paramKindIndexActivated(int);
  void playButtonPressed();
  void stopButtonPressed();
  void saveButtonPressed();
  void addUnit(int woice_idx, QString unit_name);
  void removeUnit();
  void addOverdrive();
  void removeOverdrive(int no);
  void addWoice(const AddWoice &w);
  void removeWoice(int idx);
  void changeWoice(int idx, const AddWoice &w);
  void selectWoice(int idx);
  void beatsChanged(int tempo);
  void tempoChanged(int beats);
  void followPlayheadClicked(FollowPlayhead);
  void copyChanged(bool);
  void moveUnit(bool up);
  void volumeChanged(int volume);
  void bufferLengthChanged(double secs);
  void userSelected(int user);

 public slots:
  void setQuantXIndex(int);
  void setQuantYDenom(int);
  void setParamKindIndex(int);
  // void setSelectedUnits(QList<qint32> idx);
  void setCurrentUnit(int unit_no);
  void setCurrentWoice(int unit_no);
  void setPlay(bool);
  void setModified(bool);
  void setTempo(int tempo);
  void setBeats(int beats);
  void setFollowPlayhead(FollowPlayhead follow);
  void setCopy(bool);
  void openAddUnitWindow();
  void refreshVolumeMeterShowText();

 private:
  Ui::SideMenu *ui;
  SelectWoiceDialog *m_add_unit_dialog;
  UnitListModel *m_units;
  WoiceListModel *m_woices;
  UserListModel *m_users;
  DelayEffectModel *m_delays;
  OverdriveEffectModel *m_ovdrvs;
  VolumeMeterWidget *m_volume_meter_widget;
};

#endif  // SIDEMENU_H
