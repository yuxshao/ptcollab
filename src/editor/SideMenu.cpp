#include "SideMenu.h"

#include <QDebug>
#include <QDialog>
#include <QFileDialog>
#include <QList>
#include <QMessageBox>
#include <QSettings>

#include "quantize.h"
#include "ui_SideMenu.h"

const QString WOICE_DIR_KEY("woice_dir");

static QFileDialog* make_add_woice_dialog(QWidget* parent) {
  return new QFileDialog(
      parent, parent->tr("Add voice"),
      QSettings().value(WOICE_DIR_KEY).toString(),
      parent->tr("Instruments (*.ptvoice *.ptnoise *.wav *.ogg)"));
}

SideMenu::SideMenu(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::SideMenu),
      m_users(new QStringListModel(this)),
      m_add_woice_dialog(make_add_woice_dialog(this)),
      m_add_unit_dialog(new QInputDialog(this)) {
  ui->setupUi(this);
  for (auto [label, value] : quantizeXOptions)
    ui->quantX->addItem(label, value);
  for (auto [label, value] : quantizeYOptions)
    ui->quantY->addItem(label, value);
  m_add_unit_dialog->setInputMode(QInputDialog::TextInput);
  m_add_unit_dialog->setLabelText(tr("Unit name?"));

  void (QComboBox::*signal)(int) = &QComboBox::currentIndexChanged;

  connect(ui->quantX, signal, this, &SideMenu::quantXIndexUpdated);
  connect(ui->quantY, signal, this, &SideMenu::quantYIndexUpdated);
  connect(ui->playBtn, &QPushButton::clicked, this,
          &SideMenu::playButtonPressed);
  connect(ui->stopBtn, &QPushButton::clicked, this,
          &SideMenu::stopButtonPressed);
  connect(ui->showAll, &QCheckBox::toggled, this, &SideMenu::showAllChanged);
  connect(ui->units, signal, this, &SideMenu::currentUnitChanged);
  connect(ui->saveBtn, &QPushButton::clicked, this,
          &SideMenu::saveButtonPressed);
  connect(ui->hostBtn, &QPushButton::clicked, this,
          &SideMenu::hostButtonPressed);
  connect(ui->connectBtn, &QPushButton::clicked, this,
          &SideMenu::connectButtonPressed);
  connect(ui->addUnitBtn, &QPushButton::clicked, [this]() {
    if (ui->woiceList->currentItem() == nullptr)
      QMessageBox::warning(this, tr("Cannot add unit"),
                           tr("Please select a voice first to add a unit."));
    else {
      m_add_unit_dialog->setTextValue(
          QString("u-%1").arg(ui->woiceList->currentItem()->text()));
      m_add_unit_dialog->open();
    }
  });
  connect(ui->removeUnitBtn, &QPushButton::clicked, [this]() {
    if (ui->units->count() > 0 &&
        QMessageBox::question(this, tr("Are you sure?"),
                              tr("Are you sure you want to delete the unit "
                                 "(%1)? This cannot be undone.")
                                  .arg(ui->units->currentText())) ==
            QMessageBox::Yes)
      emit removeUnit();
  });
  connect(ui->addWoiceBtn, &QPushButton::clicked, m_add_woice_dialog,
          &QDialog::show);
  connect(m_add_woice_dialog, &QFileDialog::currentChanged, this,
          &SideMenu::candidateWoiceSelected);

  connect(m_add_woice_dialog, &QDialog::accepted, this, [this]() {
    for (const auto& filename : m_add_woice_dialog->selectedFiles())
      if (filename != "") {
        QSettings().setValue(WOICE_DIR_KEY, QFileInfo(filename).absolutePath());
        emit addWoice(filename);
      }
  });

  connect(ui->removeWoiceBtn, &QPushButton::clicked, [this]() {
    int idx = ui->woiceList->currentRow();
    if (idx >= 0 && ui->woiceList->currentItem() != nullptr) {
      if (ui->woiceList->count() > 0 &&
          QMessageBox::question(
              this, tr("Are you sure?"),
              tr("Are you sure you want to delete the voice "
                 "(%1)? This cannot be undone.")
                  .arg(ui->woiceList->currentItem()->text())) ==
              QMessageBox::Yes)
        emit removeWoice(idx, ui->woiceList->currentItem()->text());
    } else
      QMessageBox::warning(this, tr("Cannot remove voice"),
                           tr("Please select a valid voice to remove."));
  });
  connect(ui->woiceList, &QListWidget::currentRowChanged, this,
          &SideMenu::selectWoice);
  connect(ui->woiceList, &QListWidget::itemActivated, [this](QListWidgetItem*) {
    emit selectWoice(ui->woiceList->currentRow());
  });
  connect(m_add_unit_dialog, &QDialog::accepted, [this]() {
    QString name = m_add_unit_dialog->textValue();
    int idx = ui->woiceList->currentRow();
    if (idx >= 0 && name != "")
      emit addUnit(idx, name);
    else
      QMessageBox::warning(this, "Invalid unit options",
                           "Name or selected instrument invalid");
  });
  ui->userList->setModel(m_users);
}

void SideMenu::setEditWidgetsEnabled(bool b) {
  // Really only this first one is necessary, since you can't add anything
  // else without it.
  ui->addWoiceBtn->setEnabled(b);
  ui->removeWoiceBtn->setEnabled(b);
  ui->addUnitBtn->setEnabled(b);
  ui->removeUnitBtn->setEnabled(b);
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
