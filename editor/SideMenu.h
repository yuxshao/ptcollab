#ifndef SIDEMENU_H
#define SIDEMENU_H

#include <QStringListModel>
#include <QWidget>

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

 public slots:
  void setQuantXIndex(int);
  void setQuantYIndex(int);
  void setShowAll(bool);
  void setUnits(std::vector<QString> const &units);
  void setSelectedUnit(int);
  void setPlay(bool);
  void setModified(bool);
  void setUserList(QList<std::pair<qint64, QString>> users);

 private:
  Ui::SideMenu *ui;
  QStringListModel *m_users;
};

#endif  // SIDEMENU_H
