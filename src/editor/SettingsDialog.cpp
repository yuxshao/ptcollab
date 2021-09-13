#include "SettingsDialog.h"

#include <QMessageBox>
#include <QtDebug>

#include "Settings.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(const MidiWrapper *midi_wrapper, QWidget *parent)
    : QDialog(parent),
      m_midi_wrapper(midi_wrapper),
      ui(new Ui::SettingsDialog) {
  ui->setupUi(this);

  connect(this, &QDialog::accepted, this, &SettingsDialog::apply);
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

  // Parse Styles
  QStringList styles;
  styles.clear();
  styles.push_front("Default");
  QDirIterator dir(qApp->applicationDirPath() + "/style",
                   QDirIterator::NoIteratorFlags);
  while (dir.hasNext()) {
    if (!dir.fileName().isEmpty() && dir.fileName() != "." &&
        dir.fileName() != "..") {
      QString targetFilename = dir.filePath() + "/" + dir.fileName() + ".qss";
      if (QFile(targetFilename).exists()) {
        styles.push_front(dir.fileName());
      }
    }
    dir.next();
  }  // Search for directories that have QSS files of the same name in them
  for (int i = ui->styleCombo->count(); i >= 0; i--) {
    ui->styleCombo->removeItem(i);
  }  // is there really not a better way to do this
  ui->styleCombo->addItems(styles);
  ui->styleCombo->setCurrentText(Settings::StyleName::get());

  // Parse Midi
  QStringList ports = m_midi_wrapper->ports();
  if (ports.length() > 0)
    ports.push_front("Select a port...");
  else
    ports.push_front("No ports found...");
  ui->midiInputPortCombo->clear();
  ui->midiInputPortCombo->addItems(ports);

  {
    auto current_port = m_midi_wrapper->currentPort();
    if (current_port.has_value())
      ui->midiInputPortCombo->setCurrentIndex(current_port.value() + 1);
  }
}
