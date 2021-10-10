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
}

void SettingsDialog::apply() {
  Settings::StyleName::set(ui->styleCombo->currentText());
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

  if (ui->midiInputPortCombo->currentIndex() > 0)
    emit midiPortSelected(ui->midiInputPortCombo->currentIndex() - 1);
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

  // Identify Styles
  // then add those names to a list for usage in the Combo Box
  ui->styleCombo->clear();
  ui->styleCombo->addItems(StyleEditor::getStyles());
  ui->styleCombo->setCurrentText(Settings::StyleName::get());

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
