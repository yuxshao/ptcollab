#include "SideMenu.h"

#include <QList>

#include "ui_SideMenu.h"

SideMenu::SideMenu(QWidget* parent) : QWidget(parent), ui(new Ui::SideMenu) {
  ui->setupUi(this);
  for (auto [label, value] : quantizeXOptions)
    ui->quantX->addItem(label, value);
  for (auto [label, value] : quantizeYOptions)
    ui->quantY->addItem(label, value);

  void (QComboBox::*signal)(int) = &QComboBox::currentIndexChanged;
  connect(ui->quantX, signal, [=](int index) {
    emit quantXUpdated(ui->quantX->itemData(index).toInt());
  });
  connect(ui->quantY, signal, [=](int index) {
    emit quantYUpdated(ui->quantY->itemData(index).toInt());
  });
  connect(ui->units, signal, this, &SideMenu::selectedUnitChanged);
}

SideMenu::~SideMenu() { delete ui; }

void SideMenu::setQuantX(int x) {
  ui->quantX->setCurrentIndex(ui->quantX->findData(x));
}
void SideMenu::setQuantY(int y) {
  ui->quantY->setCurrentIndex(ui->quantX->findData(y));
}
void SideMenu::setShowAll(bool b) {
  ui->showAll->setCheckState(b ? Qt::Checked : Qt::Unchecked);
}
void SideMenu::setUnits(std::vector<QString> const& units) {
  ui->units->clear();
  for (auto& u : units) ui->units->addItem(u);
};
void SideMenu::setSelectedUnit(int u) { ui->units->setCurrentIndex(u); }
