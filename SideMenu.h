#ifndef SIDEMENU_H
#define SIDEMENU_H

#include <QWidget>

namespace Ui {
class SideMenu;
}

static std::pair<QString, int> quantizeXOptions[] = {
    {"1", 1},   {"1/2", 2},   {"1/3", 3},   {"1/4", 4},   {"1/6", 6},
    {"1/8", 8}, {"1/12", 12}, {"1/16", 16}, {"1/24", 24}, {"1/48", 48}};

static std::pair<QString, int> quantizeYOptions[] = {
    {"1", 1}, {"1/2", 2}, {"1/3", 3}, {"1/4", 4}, {"None", 256}};

class SideMenu : public QWidget {
  Q_OBJECT

 public:
  explicit SideMenu(QWidget *parent = nullptr);
  ~SideMenu();

 signals:
  void quantXUpdated(int);
  void quantYUpdated(int);
  void selectedUnitChanged(int);
  void playButtonPressed();
  void stopButtonPressed();
  void saveButtonPressed();
  void openButtonPressed();

 public slots:
  void setQuantX(int);
  void setQuantY(int);
  void setShowAll(bool);
  void setUnits(std::vector<QString> const &units);
  void setSelectedUnit(int);
  void setPlay(bool);
  void setModified(bool);

 private:
  Ui::SideMenu *ui;
};

#endif  // SIDEMENU_H
