#include "SideMenu.h"

#include <QDebug>
#include <QDialog>
#include <QFileDialog>
#include <QList>
#include <QMessageBox>
#include <QSettings>

#include "IconHelper.h"
#include "editor/ComboOptions.h"
#include "editor/Settings.h"
#include "ui_SideMenu.h"

static QFileDialog* make_add_woice_dialog(QWidget* parent) {
  return new QFileDialog(
      parent, parent->tr("Select voice"),
      QSettings().value(WOICE_DIR_KEY).toString(),
      parent->tr("Instruments (*.ptvoice *.ptnoise *.wav *.ogg)"));
}

SideMenu::SideMenu(UnitListModel* units, WoiceListModel* woices,
                   QAbstractListModel* woices_for_add_unit,
                   DelayEffectModel* delays, OverdriveEffectModel* ovdrvs,
                   QWidget* parent)
    : QWidget(parent),
      ui(new Ui::SideMenu),
      m_users(new QStringListModel(this)),
      m_add_woice_dialog(make_add_woice_dialog(this)),
      m_change_woice_dialog(make_add_woice_dialog(this)),
      m_add_unit_dialog(new SelectWoiceDialog(woices_for_add_unit, this)),
      m_units(units),
      m_woices(woices),
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
  ui->woiceList->setModel(m_woices);
  ui->delayList->setModel(m_delays);
  ui->overdriveList->setModel(m_ovdrvs);
  ui->unitList->setItemDelegate(new UnitListDelegate);
  setPlay(false);
  for (auto* list :
       {ui->unitList, ui->woiceList, ui->delayList, ui->overdriveList}) {
    list->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    list->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
  }

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
  connect(ui->followCheckbox, &QCheckBox::toggled, this,
          &SideMenu::followChanged);
  connect(ui->copyCheckbox, &QCheckBox::toggled, this, &SideMenu::copyChanged);

  connect(ui->addWoiceBtn, &QPushButton::clicked, [this]() {
    QString dir(QSettings().value(WOICE_DIR_KEY).toString());
    if (!dir.isEmpty()) m_add_woice_dialog->setDirectory(dir);
    m_add_woice_dialog->show();
  });
  connect(ui->changeWoiceBtn, &QPushButton::clicked, [this]() {
    QString dir(QSettings().value(WOICE_DIR_KEY).toString());
    if (!dir.isEmpty()) m_change_woice_dialog->setDirectory(dir);
    if (ui->woiceList->currentIndex().row() >= 0) m_change_woice_dialog->show();
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
    int idx = ui->woiceList->currentIndex().row();
    QString name = ui->woiceList->currentIndex()
                       .siblingAtColumn(int(WoiceListColumn::Name))
                       .data()
                       .toString();
    if (idx < 0 && ui->woiceList->model()->rowCount() > 0) return;
    for (const auto& filename : m_change_woice_dialog->selectedFiles())
      if (filename != "") {
        QSettings().setValue(WOICE_DIR_KEY, QFileInfo(filename).absolutePath());
        emit changeWoice(idx, name, filename);
      }
  });

  connect(ui->removeWoiceBtn, &QPushButton::clicked, [this]() {
    int idx = ui->woiceList->currentIndex().row();
    if (idx >= 0 && ui->woiceList->model()->rowCount() > 0) {
      QString name = ui->woiceList->currentIndex()
                         .siblingAtColumn(int(WoiceListColumn::Name))
                         .data()
                         .toString();
      if (QMessageBox::question(this, tr("Are you sure?"),
                                tr("Are you sure you want to delete the voice "
                                   "(%1)? This cannot be undone.")
                                    .arg(name)) == QMessageBox::Yes)
        emit removeWoice(idx, name);
    } else
      QMessageBox::warning(this, tr("Cannot remove voice"),
                           tr("Please select a valid voice to remove."));
  });

  /*connect(ui->woiceList->selectionModel(),
          &QItemSelectionModel::currentRowChanged,
          [this](const QModelIndex& curr, const QModelIndex&) {
            emit selectWoice(curr.row());
          });*/
  connect(ui->woiceList, &QTableView::clicked,
          [this](const QModelIndex& index) {
            if (index.column() == int(WoiceListColumn::Name) ||
                index.column() == int(WoiceListColumn::Key))
              emit selectWoice(index.row());
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
  connect(ui->addOverdriveBtn, &QPushButton::clicked, this,
          &SideMenu::addOverdrive);
  connect(ui->removeOverdriveBtn, &QPushButton::clicked, [this]() {
    int ovdrv_no = ui->overdriveList->selectionModel()->currentIndex().row();
    emit removeOverdrive(ovdrv_no);
  });
  connect(ui->upUnitBtn, &QPushButton::clicked,
          [this]() { emit moveUnit(true); });
  connect(ui->downUnitBtn, &QPushButton::clicked,
          [this]() { emit moveUnit(false); });
  {
    bool ok;
    int v;
    v = QSettings().value(VOLUME_KEY).toInt(&ok);
    if (ok) ui->volumeSlider->setValue(v);
  }
  connect(ui->volumeSlider, &QSlider::valueChanged, [this](int v) {
    QSettings().setValue(VOLUME_KEY, v);
    emit volumeChanged(v);
  });
  {
    bool ok;
    double v;
    v = QSettings()
            .value(BUFFER_LENGTH_KEY, DEFAULT_BUFFER_LENGTH)
            .toDouble(&ok);
    if (ok) ui->bufferLength->setText(QString("%1").arg(v, 0, 'f', 2));
  }
  connect(ui->bufferLength, &QLineEdit::editingFinished, [this]() {
    bool ok;
    double length = ui->bufferLength->text().toDouble(&ok);
    if (ok) {
      QSettings().setValue(BUFFER_LENGTH_KEY, length);
      ui->bufferLength->clearFocus();
      emit bufferLengthChanged(length);
    }
  });
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
  ui->upUnitBtn->setEnabled(b);
  ui->downUnitBtn->setEnabled(b);
}

void SideMenu::setTab(int index) { ui->tabWidget->setCurrentIndex(index); }

SideMenu::~SideMenu() { delete ui; }

void SideMenu::setQuantXIndex(int i) { ui->quantX->setCurrentIndex(i); }
void SideMenu::setQuantYIndex(int i) { ui->quantY->setCurrentIndex(i); }

void SideMenu::setModified(bool modified) {
  if (modified)
    ui->saveBtn->setText("*");
  else
    ui->saveBtn->setText("");
}

void SideMenu::setUserList(QList<std::pair<qint64, QString>> users) {
  QList<QString> usernames;
  for (const auto& [uid, username] : users)
    usernames.append(QString("%1 (%2)").arg(username).arg(uid));
  m_users->setStringList(usernames);
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

void SideMenu::setCopy(bool copy) {
  ui->copyCheckbox->setCheckState(copy ? Qt::Checked : Qt::Unchecked);
}

void SideMenu::setParamKindIndex(int index) {
  if (ui->paramSelection->currentIndex() != index)
    ui->paramSelection->setCurrentIndex(index);
}

void SideMenu::setCurrentUnit(int u) { ui->unitList->selectRow(u); }
void SideMenu::setCurrentWoice(int u) { ui->woiceList->selectRow(u); }
void SideMenu::setPlay(bool playing) {
  if (playing) {
    ui->playBtn->setIcon(QIcon(":/icons/color/pause"));
  } else {
    ui->playBtn->setIcon(QIcon(":/icons/color/play"));
  }
  ui->stopBtn->setIcon(QIcon(":/icons/color/stop"));
  ui->saveBtn->setIcon(getIcon("save"));
  ui->upUnitBtn->setIcon(getIcon("up"));
  ui->downUnitBtn->setIcon(getIcon("down"));
}
