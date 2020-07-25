#include "SideMenu.h"

#include <QDebug>
#include <QDialog>
#include <QFileDialog>
#include <QList>
#include <QMessageBox>
#include <QSettings>

#include "editor/ComboOptions.h"
#include "ui_SideMenu.h"

const QString WOICE_DIR_KEY("woice_dir");

static QFileDialog* make_add_woice_dialog(QWidget* parent) {
  return new QFileDialog(
      parent, parent->tr("Select voice"),
      QSettings().value(WOICE_DIR_KEY).toString(),
      parent->tr("Instruments (*.ptvoice *.ptnoise *.wav *.ogg)"));
}

SideMenu::SideMenu(UnitListModel* units, DelayEffectModel* delays,
                   OverdriveEffectModel* ovdrvs, QWidget* parent)
    : QWidget(parent),
      ui(new Ui::SideMenu),
      m_users(new QStringListModel(this)),
      m_add_woice_dialog(make_add_woice_dialog(this)),
      m_change_woice_dialog(make_add_woice_dialog(this)),
      m_add_unit_dialog(new SelectWoiceDialog(this)),
      m_units(units),
      m_delays(delays),
      m_ovdrvs(ovdrvs) {
  ui->setupUi(this);
  for (auto [label, value] : quantizeXOptions)
    ui->quantX->addItem(label, value);
  for (auto [label, value] : quantizeYOptions)
    ui->quantY->addItem(label, value);
  for (auto [label, value] : paramOptions)
    ui->paramSelection->addItem(label, value);

  ui->unitList->setModel(m_units);
  ui->delayList->setModel(m_delays);
  ui->overdriveList->setModel(m_ovdrvs);
  ui->unitList->setItemDelegate(new UnitListDelegate);
  ui->unitList->verticalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  ui->unitList->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  ui->delayList->verticalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  ui->delayList->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  ui->overdriveList->verticalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  ui->overdriveList->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);

  void (QComboBox::*indexChanged)(int) = &QComboBox::currentIndexChanged;

  connect(ui->quantX, indexChanged, this, &SideMenu::quantXIndexUpdated);
  connect(ui->quantY, indexChanged, this, &SideMenu::quantYIndexUpdated);
  connect(ui->playBtn, &QPushButton::clicked, this,
          &SideMenu::playButtonPressed);
  connect(ui->stopBtn, &QPushButton::clicked, this,
          &SideMenu::stopButtonPressed);
  connect(ui->unitList->selectionModel(),
          &QItemSelectionModel::currentRowChanged,
          [this](const QModelIndex& current, const QModelIndex& previous) {
            (void)previous;
            emit currentUnitChanged(current.row());
          });
  connect(ui->saveBtn, &QPushButton::clicked, this,
          &SideMenu::saveButtonPressed);
  connect(ui->hostBtn, &QPushButton::clicked, this,
          &SideMenu::hostButtonPressed);
  connect(ui->connectBtn, &QPushButton::clicked, this,
          &SideMenu::connectButtonPressed);
  connect(ui->addUnitBtn, &QPushButton::clicked, m_add_unit_dialog,
          &QDialog::open);
  connect(ui->removeUnitBtn, &QPushButton::clicked, [this]() {
    QVariant data = ui->unitList->currentIndex()
                        .siblingAtColumn(int(UnitListColumn::Name))
                        .data();
    if (data.canConvert<QString>() &&
        QMessageBox::question(this, tr("Are you sure?"),
                              tr("Are you sure you want to delete the unit "
                                 "(%1)? This cannot be undone.")
                                  .arg(data.value<QString>())) ==
            QMessageBox::Yes)
      emit removeUnit();
  });
  connect(ui->setRepeatBtn, &QPushButton::clicked, this, &SideMenu::setRepeat);
  connect(ui->setLastBtn, &QPushButton::clicked, this, &SideMenu::setLast);
  connect(ui->followCheckbox, &QCheckBox::toggled, this, &SideMenu::setFollow);

  connect(ui->addWoiceBtn, &QPushButton::clicked, [this]() {
    QString dir(QSettings().value(WOICE_DIR_KEY).toString());
    if (!dir.isEmpty()) m_add_woice_dialog->setDirectory(dir);
    m_add_woice_dialog->show();
  });
  connect(ui->changeWoiceBtn, &QPushButton::clicked, [this]() {
    QString dir(QSettings().value(WOICE_DIR_KEY).toString());
    if (!dir.isEmpty()) m_add_woice_dialog->setDirectory(dir);
    if (ui->woiceList->currentRow() >= 0) m_change_woice_dialog->show();
  });
  connect(m_add_woice_dialog, &QFileDialog::currentChanged, this,
          &SideMenu::candidateWoiceSelected);
  connect(m_change_woice_dialog, &QFileDialog::currentChanged, this,
          &SideMenu::candidateWoiceSelected);

  connect(m_add_woice_dialog, &QDialog::accepted, this, [this]() {
    for (const auto& filename : m_add_woice_dialog->selectedFiles())
      if (filename != "") {
        QSettings().setValue(WOICE_DIR_KEY, QFileInfo(filename).absolutePath());
        emit addWoice(filename);
      }
  });

  connect(ui->tempoField, &QLineEdit::editingFinished, [this]() {
    bool ok;
    int tempo = ui->tempoField->text().toInt(&ok);
    if (!ok || tempo < 20 || tempo > 600)
      QMessageBox::critical(this, tr("Invalid tempo"),
                            tr("Tempo must be number between 20 and 600"));
    else {
      ui->tempoField->setText(QString(tempo));
      emit tempoChanged(tempo);
    }
  });

  connect(ui->beatsField, &QLineEdit::editingFinished, [this]() {
    bool ok;
    int beats = ui->beatsField->text().toInt(&ok);
    if (!ok || beats < 1 || beats > 16)
      QMessageBox::critical(this, tr("Invalid beats"),
                            tr("Beats must be number between 1 and 16"));
    else {
      setBeats(beats);
      emit beatsChanged(beats);
    }
  });

  connect(m_change_woice_dialog, &QDialog::accepted, this, [this]() {
    int idx = ui->woiceList->currentRow();
    if (idx < 0 || !ui->woiceList->currentItem()) return;
    for (const auto& filename : m_change_woice_dialog->selectedFiles())
      if (filename != "") {
        QSettings().setValue(WOICE_DIR_KEY, QFileInfo(filename).absolutePath());
        emit changeWoice(idx, ui->woiceList->currentItem()->text(), filename);
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
    QString name = m_add_unit_dialog->getUnitNameSelection();
    int idx = m_add_unit_dialog->getSelectedWoiceIndex();
    if (idx >= 0 && name != "")
      emit addUnit(idx, name);
    else
      QMessageBox::warning(this, "Invalid unit options",
                           "Name or selected instrument invalid");
  });
  ui->userList->setModel(m_users);

  connect(ui->paramSelection, indexChanged, this,
          &SideMenu::paramKindIndexChanged);
}

void SideMenu::setEditWidgetsEnabled(bool b) {
  // Really only this first one is necessary, since you can't add anything
  // else without it.
  ui->addWoiceBtn->setEnabled(b);
  ui->changeWoiceBtn->setEnabled(b);
  ui->removeWoiceBtn->setEnabled(b);
  ui->addUnitBtn->setEnabled(b);
  ui->removeUnitBtn->setEnabled(b);
  ui->tempoField->setEnabled(b);
  ui->beatsField->setEnabled(b);
}

SideMenu::~SideMenu() { delete ui; }

void SideMenu::setQuantXIndex(int i) { ui->quantX->setCurrentIndex(i); }
void SideMenu::setQuantYIndex(int i) { ui->quantY->setCurrentIndex(i); }

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
  m_add_unit_dialog->setWoices(woices);
}

void SideMenu::setTempo(int tempo) {
  ui->tempoField->setText(QString("%1").arg(tempo));
}
void SideMenu::setBeats(int beats) {
  ui->beatsField->setText(QString("%1").arg(beats));
}

void SideMenu::setFollow(bool follow) {
  ui->followCheckbox->setCheckState(follow ? Qt::Checked : Qt::Unchecked);
}

void SideMenu::setParamKindIndex(int index) {
  if (ui->paramSelection->currentIndex() != index)
    ui->paramSelection->setCurrentIndex(index);
}

void SideMenu::setCurrentUnit(int u) { ui->unitList->selectRow(u); }
void SideMenu::setPlay(bool playing) {
  if (playing)
    ui->playBtn->setText("Pause (SPC)");
  else
    ui->playBtn->setText("Play (SPC)");
}
