#include "SideMenu.h"

#include <QDebug>
#include <QDialog>
#include <QFileDialog>
#include <QList>
#include <QMessageBox>
#include <QSettings>

#include "IconHelper.h"
#include "VolumeMeterWidget.h"
#include "editor/ComboOptions.h"
#include "editor/Settings.h"
#include "ui_SideMenu.h"

SideMenu::SideMenu(UnitListModel* units, WoiceListModel* woices,
                   UserListModel* users, SelectWoiceDialog* add_unit_dialog,
                   DelayEffectModel* delays, OverdriveEffectModel* ovdrvs,
                   NewWoiceDialog* new_woice_dialog,
                   NewWoiceDialog* change_woice_dialog,
                   VolumeMeterFrame* volume_meter_frame, QWidget* parent)
    : QWidget(parent),
      ui(new Ui::SideMenu),
      m_add_unit_dialog(add_unit_dialog),
      m_units(units),
      m_woices(woices),
      m_users(users),
      m_delays(delays),
      m_ovdrvs(ovdrvs) {
  ui->setupUi(this);
  m_volume_meter_widget = new VolumeMeterWidget(volume_meter_frame, this);
  QLayoutItem* previous =
      layout()->replaceWidget(ui->volumeMeterWidget, m_volume_meter_widget);
  previous->widget()->deleteLater();
  m_volume_meter_widget->setSizePolicy(
      QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
  m_volume_meter_widget->setParent(this);

  for (auto [label, value] : quantizeXOptions())
    ui->quantX->addItem(label, value);
  updateQuantizeYOptions(EditState().m_quantize_pitch_denom);
  for (auto [label, value] : paramOptions())
    ui->paramSelection->addItem(label, value);

  ui->unitList->setModel(m_units);
  ui->woiceList->setModel(m_woices);
  ui->usersList->setModel(m_users);
  ui->delayList->setModel(m_delays);
  ui->overdriveList->setModel(m_ovdrvs);
  m_unit_list_delegate = new UnitListDelegate(ui->unitList->selectionModel());
  ui->unitList->setItemDelegate(m_unit_list_delegate);
  connect(ui->unitList, &TableView::hoveredRowChanged, this,
          &SideMenu::hoveredUnitChanged);
  setPlay(false);
  for (auto* list : {(QTableView*)ui->unitList, ui->woiceList, ui->delayList,
                     ui->overdriveList, ui->usersList}) {
    list->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    list->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
  }
  ui->usersList->horizontalHeader()->setSectionResizeMode(
      int(UserListModel::Column::Name), QHeaderView::Stretch);

  void (QComboBox::*comboItemActivated)(int) = &QComboBox::activated;

  connect(ui->quantX, comboItemActivated, this,
          &SideMenu::quantXIndexActivated);
  connect(ui->quantY, comboItemActivated, this, [this](int index) {
    emit quantYDenomActivated(ui->quantY->itemData(index).toInt());
  });
  connect(ui->playBtn, &QPushButton::clicked, this,
          &SideMenu::playButtonPressed);
  connect(ui->stopBtn, &QPushButton::clicked, this,
          &SideMenu::stopButtonPressed);
  connect(ui->unitList->selectionModel(),
          &QItemSelectionModel::currentRowChanged,
          [this](const QModelIndex& current, const QModelIndex& previous) {
            (void)previous;
            ui->unitList->updateRow(previous.row());
            ui->unitList->updateRow(current.row());
            if (current.isValid()) emit currentUnitChanged(current.row());
          });
  connect(ui->unitList->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          &SideMenu::selectedUnitsChanged);
  connect(ui->unitList, &QTableView::clicked,
          [this](const QModelIndex& index) { emit unitClicked(index.row()); });
  connect(ui->saveBtn, &QPushButton::clicked, this,
          &SideMenu::saveButtonPressed);
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
  connect(ui->followCheckbox, &QCheckBox::clicked, [this](bool is_checked) {
    emit followPlayheadClicked(is_checked ? FollowPlayhead::Jump
                                          : FollowPlayhead::None);
  });
  connect(ui->copyCheckbox, &QCheckBox::toggled, this, &SideMenu::copyChanged);

  connect(ui->addWoiceBtn, &QPushButton::clicked, new_woice_dialog,
          &QDialog::show);
  connect(ui->changeWoiceBtn, &QPushButton::clicked,
          [this, change_woice_dialog]() {
            if (ui->woiceList->currentIndex().row() >= 0)
              change_woice_dialog->show();
          });

  connect(change_woice_dialog, &QDialog::accepted, this,
          [this, change_woice_dialog]() {
            if (!ui->woiceList->currentIndex().isValid()) return;
            int idx = ui->woiceList->currentIndex().row();
            if (idx < 0 && ui->woiceList->model()->rowCount() > 0) return;
            const auto& woices = change_woice_dialog->selectedWoices();
            if (woices.size() > 0) emit changeWoice(idx, woices[0]);
          });
  connect(new_woice_dialog, &QDialog::accepted, this,
          [this, new_woice_dialog]() {
            emit addWoices(new_woice_dialog->selectedWoices());
          });

  connect(ui->tempoSpin, &QSpinBox::editingFinished,
          [this]() { emit tempoChanged(ui->tempoSpin->value()); });
  connect(ui->beatsSpin, &QSpinBox::editingFinished,
          [this]() { emit beatsChanged(ui->beatsSpin->value()); });

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
        emit removeWoice(idx);
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

  connect(ui->paramSelection, comboItemActivated, this,
          &SideMenu::paramKindIndexActivated);
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
  connect(ui->usersList, &QTableView::activated,
          [this](const QModelIndex& index) { emit userSelected(index.row()); });
  connect(ui->watchBtn, &QPushButton::clicked, [this]() {
    if (ui->usersList->selectionModel()->hasSelection())
      emit userSelected(ui->usersList->currentIndex().row());
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
  ui->tempoSpin->setEnabled(b);
  ui->beatsSpin->setEnabled(b);
  ui->upUnitBtn->setEnabled(b);
  ui->downUnitBtn->setEnabled(b);
}

void SideMenu::setTab(int index) { ui->tabWidget->setCurrentIndex(index); }

SideMenu::~SideMenu() { delete ui; }

void SideMenu::setQuantXIndex(int i) { ui->quantX->setCurrentIndex(i); }
void SideMenu::setQuantYDenom(int denom) {
  int i;
  for (i = 0; i < ui->quantY->count() - 1; ++i)
    if (ui->quantY->itemData(i).toInt() == denom) break;
  ui->quantY->setCurrentIndex(i);
}

void SideMenu::setModified(bool modified) {
  if (modified)
    ui->saveBtn->setText("*");
  else
    ui->saveBtn->setText("");
}

void SideMenu::setTempo(int tempo) { ui->tempoSpin->setValue(tempo); }
void SideMenu::setBeats(int beats) { ui->beatsSpin->setValue(beats); }

void SideMenu::setFollowPlayhead(FollowPlayhead follow) {
  ui->followCheckbox->setCheckState(follow == FollowPlayhead::None
                                        ? Qt::CheckState::Unchecked
                                        : Qt::CheckState::Checked);
}

void SideMenu::setCopy(bool copy) {
  ui->copyCheckbox->setCheckState(copy ? Qt::Checked : Qt::Unchecked);
}

void SideMenu::openAddUnitWindow() { m_add_unit_dialog->show(); }

void SideMenu::refreshVolumeMeterShowText() {
  m_volume_meter_widget->refreshShowText();
}

void SideMenu::updateQuantizeYOptions(int currentDenom) {
  const auto& options = Settings::AdvancedQuantizeY::get()
                            ? quantizeYOptionsAdvanced()
                            : quantizeYOptionsSimple();
  ui->quantY->clear();
  for (auto& [label, value] : options) ui->quantY->addItem(label, value);
  setQuantYDenom(currentDenom);
}

void SideMenu::setParamKindIndex(int index) {
  if (ui->paramSelection->currentIndex() != index)
    ui->paramSelection->setCurrentIndex(index);
}

void SideMenu::setCurrentUnit(int u) {
  QModelIndex index =
      ui->unitList->model()->index(u, int(UnitListColumn::Name));

  QItemSelectionModel* selection = ui->unitList->selectionModel();
  selection->setCurrentIndex(
      index, QItemSelectionModel::Current | QItemSelectionModel::Rows);
}

void SideMenu::setFocusedUnit(std::optional<int> unit_no) {
  std::optional<int> old_unit_no = m_unit_list_delegate->hover_unit_no;
  m_unit_list_delegate->hover_unit_no = unit_no;
  if (old_unit_no != unit_no) {
    if (old_unit_no.has_value()) ui->unitList->updateRow(old_unit_no.value());
    if (unit_no.has_value()) ui->unitList->updateRow(unit_no.value());
  }
}

void SideMenu::setCurrentWoice(int u) { ui->woiceList->selectRow(u); }

void SideMenu::setUnitSelected(int u, bool selected) {
  ui->unitList->selectionModel()->select(
      ui->unitList->model()->index(u, int(UnitListColumn::Name)),
      (selected ? QItemSelectionModel::Select : QItemSelectionModel::Deselect) |
          QItemSelectionModel::Rows);
}
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
