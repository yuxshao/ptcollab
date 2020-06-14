#include "SideMenu.h"

#include <QDebug>
#include <QDialog>
#include <QList>
#include <QMessageBox>

#include "quantize.h"
#include "ui_SideMenu.h"
SideMenu::SideMenu(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::SideMenu),
      m_users(new QStringListModel(this)),
      m_add_unit_dialog(new SelectWoiceDialog(this)) {
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
  connect(ui->addUnitBtn, &QPushButton::clicked, m_add_unit_dialog,
          &QDialog::open);
  connect(ui->removeUnitBtn, &QPushButton::clicked, [this]() {
    if (ui->units->count() > 0 &&
        QMessageBox::question(this, tr("Are you sure?"),
                              tr("Are you sure you want to delete the unit "
                                 "(%1)? This cannot be undone.")
                                  .arg(ui->units->currentText())) ==
            QMessageBox::Yes)
      emit removeUnit();
  });
  connect(m_add_unit_dialog, &QDialog::accepted, [this]() {
    int idx = m_add_unit_dialog->getSelectedWoiceIndex();
    QString name = m_add_unit_dialog->getUnitNameSelection();
    if (idx >= 0 && name != "")
      emit addUnit(idx, name);
    else
      QMessageBox::warning(this, "Invalid unit options",
                           "Name or selected instrument invalid");
  });
  ui->userList->setModel(m_users);
}

SideMenu::~SideMenu() { delete ui; }

void SideMenu::setQuantXIndex(int i) { ui->quantX->setCurrentIndex(i); }
void SideMenu::setQuantYIndex(int i) { ui->quantY->setCurrentIndex(i); }
void SideMenu::setShowAll(bool b) {
  ui->showAll->setCheckState(b ? Qt::Checked : Qt::Unchecked);
}
void SideMenu::setUnits(std::vector<QString> const& units) {
  unsigned long long index = ui->units->currentIndex();
  ui->units->clear();
  for (auto& u : units) ui->units->addItem(u);
  if (index < units.size()) ui->units->setCurrentIndex(index);
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

void SideMenu::setWoiceList(QStringList woices) {
  m_add_unit_dialog->setWoices(woices);
  ui->woiceList->clear();
  ui->woiceList->addItems(woices);
}

void SideMenu::setSelectedUnit(int u) { ui->units->setCurrentIndex(u); }
void SideMenu::setPlay(bool playing) {
  if (playing)
    ui->playBtn->setText("Pause (SPC)");
  else
    ui->playBtn->setText("Play (SPC)");
}
