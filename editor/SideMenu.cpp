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
  connect(ui->playBtn, &QPushButton::clicked, this,
          &SideMenu::playButtonPressed);
  connect(ui->stopBtn, &QPushButton::clicked, this,
          &SideMenu::stopButtonPressed);
  connect(ui->units, signal, this, &SideMenu::selectedUnitChanged);
  connect(ui->saveBtn, &QPushButton::clicked, this,
          &SideMenu::saveButtonPressed);
  connect(ui->openBtn, &QPushButton::clicked, this,
          &SideMenu::openButtonPressed);
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
void SideMenu::setModified(bool modified) {
  if (modified)
    ui->saveBtn->setText("Save* (C-s)");
  else
    ui->saveBtn->setText("Save (C-s)");
}
void SideMenu::setSelectedUnit(int u) { ui->units->setCurrentIndex(u); }
void SideMenu::setPlay(bool playing) {
  if (playing)
    ui->playBtn->setText("Pause (SPC)");
  else
    ui->playBtn->setText("Play (SPC)");
}
