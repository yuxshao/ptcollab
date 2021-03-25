#include "SettingsDialog.h"

#include <QMessageBox>
#include <QStyleFactory>
#include <QCoreApplication>
#include <QFile>
#include <QDebug>

#include "Settings.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(const MidiWrapper *midi_wrapper, QWidget *parent)
    : QDialog(parent),
      m_midi_wrapper(midi_wrapper),
      ui(new Ui::SettingsDialog) {
  ui->setupUi(this);

  const QStringList styles = QStyleFactory::keys();

  int styleIterator = 0;
  int styleIndex = 0;
  styleFile.open(QFile::ReadOnly);
  QString currentStyle = styleFile.readAll();
  while(!(styles.count() == styleIterator))
  {
      ui->styleCombo->addItem(styles.at(styleIterator));
      if(styles.at(styleIterator) == currentStyle)
      {
          styleIndex = styleIterator;
      }
      styleIterator++;
  }
  styleFile.close();
  ui->styleCombo->setCurrentIndex(styleIndex);

  if(Settings::CustomStyle::get())
  {
      ui->styleCombo->setEnabled(false);
  }

  connect(this, &QDialog::accepted, this, &SettingsDialog::apply);
  QObject::connect(ui->customStyleCheck, SIGNAL(released()), SLOT(styleTickBox()));
}

void SettingsDialog::apply() {
  Settings::ChordPreview::set(ui->chordPreviewCheck->isChecked());
  Settings::CustomStyle::set(ui->customStyleCheck->isChecked());
  Settings::SpacebarStop::set(ui->pauseReseekCheck->isChecked());
  Settings::VelocityDrag::set(ui->velocityDragCheck->isChecked());
  Settings::SwapScrollOrientation::set(
      ui->swapScrollOrientationCheck->isChecked());
  Settings::SwapZoomOrientation::set(ui->swapZoomOrientationCheck->isChecked());
  Settings::AutoAddUnit::set(ui->autoAddUnitCheck->isChecked());
  Settings::UnitPreviewClick::set(ui->unitPreviewClickCheck->isChecked());
  Settings::AutoAdvance::set(ui->autoAdvanceCheck->isChecked());
  Settings::PolyphonicMidiNotePreview::set(ui->polyphonicMidiNotePreviewCheck->isChecked());

  styleFile.remove();
  styleFile.open(QFile::ReadWrite);
  styleFile.write(ui->styleCombo->currentText().toUtf8());
  styleFile.close();

  if (ui->midiInputPortCombo->currentIndex() > 0)
    emit midiPortSelected(ui->midiInputPortCombo->currentIndex() - 1);
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::showEvent(QShowEvent *) {
  ui->chordPreviewCheck->setChecked(Settings::ChordPreview::get());
  ui->customStyleCheck->setChecked(Settings::CustomStyle::get());
  ui->pauseReseekCheck->setChecked(Settings::SpacebarStop::get());
  ui->velocityDragCheck->setChecked(Settings::VelocityDrag::get());
  ui->swapScrollOrientationCheck->setChecked(
      Settings::SwapScrollOrientation::get());
  ui->swapZoomOrientationCheck->setChecked(
      Settings::SwapZoomOrientation::get());
  ui->autoAddUnitCheck->setChecked(Settings::AutoAddUnit::get());
  ui->unitPreviewClickCheck->setChecked(Settings::UnitPreviewClick::get());
  ui->autoAdvanceCheck->setChecked(Settings::AutoAdvance::get());
  ui->polyphonicMidiNotePreviewCheck->setChecked(Settings::PolyphonicMidiNotePreview::get());

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

void SettingsDialog::styleTickBox()
{
    if(ui->customStyleCheck->isChecked())
    {
        ui->styleCombo->setEnabled(false);
    }
    else
    {
        ui->styleCombo->setEnabled(true);
    }
}
