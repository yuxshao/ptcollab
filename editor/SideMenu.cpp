#include "SideMenu.h"

#include <QList>

#include "quantize.h"
#include "ui_SideMenu.h"

SideMenu::SideMenu(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::SideMenu),
      m_users(new QStringListModel(this)) {
  ui->setupUi(this);
  for (auto [label, value] : quantizeXOptions)
    ui->quantX->addItem(label, value);
  for (auto [label, value] : quantizeYOptions)
    ui->quantY->addItem(label, value);

  void (QComboBox::*signal)(int) = &QComboBox::currentIndexChanged;

  connect(ui->quantX, signal, this, &SideMenu::quantXIndexUpdated);
  connect(ui->quantY, signal, this, &SideMenu::quantYIndexUpdated);
  connect(ui->playBtn, &QPushButton::clicked, this,
          &SideMenu::playButtonPressed);
  connect(ui->stopBtn, &QPushButton::clicked, this,
          &SideMenu::stopButtonPressed);
  connect(ui->showAll, &QCheckBox::toggled, this, &SideMenu::showAllChanged);
  connect(ui->units, signal, this, &SideMenu::selectedUnitChanged);
  connect(ui->saveBtn, &QPushButton::clicked, this,
          &SideMenu::saveButtonPressed);
  connect(ui->hostBtn, &QPushButton::clicked, this,
          &SideMenu::hostButtonPressed);
  connect(ui->connectBtn, &QPushButton::clicked, this,
          &SideMenu::connectButtonPressed);
  ui->userList->setModel(m_users);
}

SideMenu::~SideMenu() { delete ui; }

void SideMenu::setQuantXIndex(int i) { ui->quantX->setCurrentIndex(i); }
void SideMenu::setQuantYIndex(int i) { ui->quantY->setCurrentIndex(i); }
void SideMenu::setShowAll(bool b) {
  ui->showAll->setCheckState(b ? Qt::Checked : Qt::Unchecked);
}
void SideMenu::setUnits(std::vector<QString> const& units) {
  ui->units->clear();
  for (auto& u : units) ui->units->addItem(u);
};
void SideMenu::setModified(bool modified) {
  if (modified)
    ui->saveBtn->setText("Save locally* (C-s)");
  else
    ui->saveBtn->setText("Save locally (C-s)");
}

void SideMenu::setUserList(QList<std::pair<qint64, QString>> users) {
  QList<QString> usernames;
  for (const auto& [uid, username] : users)
    usernames.append(QString("%1 (%2)").arg(username).arg(uid));
  m_users->setStringList(usernames);
}
void SideMenu::setSelectedUnit(int u) { ui->units->setCurrentIndex(u); }
void SideMenu::setPlay(bool playing) {
  if (playing)
    ui->playBtn->setText("Pause (SPC)");
  else
    ui->playBtn->setText("Play (SPC)");
}
