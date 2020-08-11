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

namespace Ui {
class SideMenu;
}

// TODO: Add a PxtoneSideMenu that takes a client and routes the signals /
// slots?
class SideMenu : public QWidget {
  Q_OBJECT

 public:
  explicit SideMenu(UnitListModel *units, DelayEffectModel *delays,
                    OverdriveEffectModel *ovdrvs, QWidget *parent = nullptr);
  void setEditWidgetsEnabled(bool);
  ~SideMenu();

 signals:
  void quantXIndexUpdated(int);
  void quantYIndexUpdated(int);
  void currentUnitChanged(int);
  void paramKindIndexChanged(int);
  void playButtonPressed();
  void stopButtonPressed();
  void saveButtonPressed();
  void hostButtonPressed();
  void connectButtonPressed();
  void addUnit(int woice_idx, QString unit_name);
  void removeUnit();
  void addOverdrive();
  void removeOverdrive(int no);
  // TODO: Maybe set up the UI so that you can also give a name
  void addWoice(QString filename);
  void removeWoice(int idx, QString name);
  void changeWoice(int idx, QString name, QString filename);
  void selectWoice(int idx);
  void candidateWoiceSelected(QString filename);
  void selectedUnitsChanged(QList<qint32> idx);
  void beatsChanged(int tempo);
  void tempoChanged(int beats);
  void setRepeat();
  void setLast();
  void followChanged(bool);
  void copyChanged(bool);

 public slots:
  void setQuantXIndex(int);
  void setQuantYIndex(int);
  void setParamKindIndex(int);
  // void setSelectedUnits(QList<qint32> idx);
  void setCurrentUnit(int unit_no);
  void setCurrentWoice(int unit_no);
  void setPlay(bool);
  void setModified(bool);
  void setUserList(QList<std::pair<qint64, QString>> users);
  void setWoiceList(QStringList);
  void setTempo(int tempo);
  void setBeats(int beats);
  void setFollow(bool follow);
  void setCopy(bool);
  void setSetOrClearMeas(bool);

 private:
  Ui::SideMenu *ui;
  QStringListModel *m_users;
  QFileDialog *m_add_woice_dialog;
  QFileDialog *m_change_woice_dialog;
  SelectWoiceDialog *m_add_unit_dialog;
  UnitListModel *m_units;
  DelayEffectModel *m_delays;
  OverdriveEffectModel *m_ovdrvs;
  bool m_middle_of_set_woice_list;
};

#endif  // SIDEMENU_H
