#include "SettingsDialog.h"

#include <QDesktopServices>
#include <QUrl>
#include <QtDebug>

#include "Settings.h"
#include "StyleEditor.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(const MidiWrapper *midi_wrapper, QWidget *parent)
    : QDialog(parent),
      m_midi_wrapper(midi_wrapper),
      ui(new Ui::SettingsDialog) {
  ui->setupUi(this);

  connect(this, &QDialog::accepted, this, &SettingsDialog::apply);
  connect(ui->styleButton, &QPushButton::released,
          []() { StyleEditor::initializeStyleDir(); });
  connect(ui->alternateTuningCheck, &QCheckBox::stateChanged,
          ui->alternateTuningSystemContainer, &QWidget::setVisible);
}

void SettingsDialog::apply() {
  Settings::StyleName::set(ui->styleCombo->currentText());
  Settings::Language::set(ui->languageCombo->currentText());
  Settings::ChordPreview::set(ui->chordPreviewCheck->isChecked());
  Settings::SpacebarStop::set(ui->pauseReseekCheck->isChecked());
  Settings::VelocityDrag::set(ui->velocityDragCheck->isChecked());
  Settings::SwapScrollOrientation::set(
      ui->swapScrollOrientationCheck->isChecked());
  Settings::SwapZoomOrientation::set(ui->swapZoomOrientationCheck->isChecked());
  Settings::AutoAddUnit::set(ui->autoAddUnitCheck->isChecked());
  Settings::UnitPreviewClick::set(ui->unitPreviewClickCheck->isChecked());
  Settings::AutoAdvance::set(ui->autoAdvanceCheck->isChecked());
  Settings::PolyphonicMidiNotePreview::set(
      ui->polyphonicMidiNotePreviewCheck->isChecked());
  Settings::ShowWelcomeDialog::set(ui->showWelcomeDialogCheck->isChecked());
  Settings::ShowVolumeMeterLabels::set(
      ui->showVolumeMeterLabelsCheck->isChecked());
  Settings::AutoConnectMidi::set(ui->autoConnectMidiCheck->isChecked());
  Settings::OctaveDisplayA::set(ui->octaveMarkerACheck->isChecked());
  Settings::PinnedUnitLabels::set(ui->pinnedUnitLabelCheck->isChecked());
  Settings::AdvancedQuantizeY::set(ui->alternateTuningCheck->isChecked());
  Settings::RecordMidi::set(ui->recordMidiCheck->isChecked());
  Settings::NewUnitDefaultVolume::set(ui->defaultVolumeSpin->value());
  Settings::MeasureViewClickToJumpUnit::set(
      ui->selectPinnedUnitOnClickCheck->isChecked());
  Settings::StrictFollowSeek::set(ui->followSeekStrictCheck->isChecked());
  Settings::VelocitySensitivity::set(ui->velocitySensitivityCheck->isChecked());
  Settings::DisplayScale::set(ui->displayScaleSpin->value());
  Settings::LeftPianoWidth::set(ui->leftPianoWidthSpin->value());
  if (ui->alternateTuningCheck->isChecked()) {
    QList<int> rowDisplayPattern;
    for (char c : ui->rowDisplayEdit->text().toStdString()) {
      if (rowDisplayPattern.size() >= 35) break;
      if (c == 'b' || c == 'B')
        rowDisplayPattern.push_back(1);
      else if (c == 'w' || c == 'W')
        rowDisplayPattern.push_back(0);
    }
    if (rowDisplayPattern.size() < 1) rowDisplayPattern = {0, 1};
    Settings::DisplayEdo::set(rowDisplayPattern);
  } else
    Settings::DisplayEdo::clear();

  emit quantYOptionsChanged();
  if (ui->midiInputPortCombo->currentIndex() >= 0)
    emit midiPortSelected(ui->midiInputPortCombo->currentIndex() - 1,
                          ui->midiInputPortCombo->currentText());
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::showEvent(QShowEvent *) {
  ui->chordPreviewCheck->setChecked(Settings::ChordPreview::get());
  ui->pauseReseekCheck->setChecked(Settings::SpacebarStop::get());
  ui->velocityDragCheck->setChecked(Settings::VelocityDrag::get());
  ui->swapScrollOrientationCheck->setChecked(
      Settings::SwapScrollOrientation::get());
  ui->swapZoomOrientationCheck->setChecked(
      Settings::SwapZoomOrientation::get());
  ui->autoAddUnitCheck->setChecked(Settings::AutoAddUnit::get());
  ui->unitPreviewClickCheck->setChecked(Settings::UnitPreviewClick::get());
  ui->autoAdvanceCheck->setChecked(Settings::AutoAdvance::get());
  ui->polyphonicMidiNotePreviewCheck->setChecked(
      Settings::PolyphonicMidiNotePreview::get());
  ui->showWelcomeDialogCheck->setChecked(Settings::ShowWelcomeDialog::get());
  ui->showVolumeMeterLabelsCheck->setChecked(
      Settings::ShowVolumeMeterLabels::get());
  ui->alternateTuningCheck->setChecked(Settings::AdvancedQuantizeY::get());
  ui->autoConnectMidiCheck->setChecked(Settings::AutoConnectMidi::get());
  ui->alternateTuningCheck->setChecked(Settings::AdvancedQuantizeY::get());
  ui->alternateTuningSystemContainer->setVisible(
      ui->alternateTuningCheck->isChecked());
  ui->octaveMarkerACheck->setChecked(Settings::OctaveDisplayA::get());
  ui->pinnedUnitLabelCheck->setChecked(Settings::PinnedUnitLabels::get());
  ui->selectPinnedUnitOnClickCheck->setChecked(
      Settings::MeasureViewClickToJumpUnit::get());
  ui->defaultVolumeSpin->setValue(Settings::NewUnitDefaultVolume::get());
  ui->displayScaleSpin->setValue(Settings::DisplayScale::get());
  ui->leftPianoWidthSpin->setValue(Settings::LeftPianoWidth::get());
  ui->recordMidiCheck->setChecked(Settings::RecordMidi::get());
  ui->followSeekStrictCheck->setChecked(Settings::StrictFollowSeek::get());
  ui->velocitySensitivityCheck->setChecked(
      Settings::VelocitySensitivity::get());

  QString rowDisplay = "";
  for (auto &b : Settings::DisplayEdo::get()) rowDisplay += (b ? "B" : "W");
  ui->rowDisplayEdit->setText(rowDisplay);

  // Identify Styles then add those names to a list for usage in the Combo Box
  ui->styleCombo->clear();
  ui->styleCombo->addItems(StyleEditor::getStyles());
  ui->styleCombo->setCurrentText(Settings::StyleName::get());

  // Identify languages similarly
  ui->languageCombo->clear();
  ui->languageCombo->addItem("Default");
  ui->languageCombo->addItems(QDir(":/i18n/").entryList(
      QDir::Files | QDir::NoDotAndDotDot | QDir::Readable));
  ui->languageCombo->setCurrentText(Settings::Language::get());

  // Identify MIDI ports
  QStringList ports = m_midi_wrapper->ports();
  ports.push_front(m_midi_wrapper->portDropdownMessage());
  ui->midiInputPortCombo->clear();
  ui->midiInputPortCombo->addItems(ports);

  {
    auto current_port = m_midi_wrapper->currentPort();
    if (current_port.has_value())
      ui->midiInputPortCombo->setCurrentIndex(current_port.value() + 1);
  }
}
