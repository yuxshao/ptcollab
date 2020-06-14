#ifndef SIDEMENU_H
#define SIDEMENU_H

#include <QStringListModel>
#include <QWidget>

#include "SelectWoiceDialog.h"

namespace Ui {
class SideMenu;
}

class SideMenu : public QWidget {
  Q_OBJECT

 public:
  explicit SideMenu(QWidget *parent = nullptr);
  ~SideMenu();

 signals:
  void quantXIndexUpdated(int);
  void quantYIndexUpdated(int);
  void selectedUnitChanged(int);
  void showAllChanged(bool);
  void playButtonPressed();
  void stopButtonPressed();
  void saveButtonPressed();
  void hostButtonPressed();
  void connectButtonPressed();
  void addUnit(int woice_idx, QString unit_name);
  void removeUnit();

 public slots:
  void setQuantXIndex(int);
  void setQuantYIndex(int);
  void setShowAll(bool);
  void setUnits(std::vector<QString> const &units);
  void setSelectedUnit(int);
  void setPlay(bool);
  void setModified(bool);
  void setUserList(QList<std::pair<qint64, QString>> users);
  void setWoiceList(QStringList);

 private:
  Ui::SideMenu *ui;
  QStringListModel *m_users;
  SelectWoiceDialog *m_add_unit_dialog;
};

#endif  // SIDEMENU_H
