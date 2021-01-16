#include "SettingsDialog.h"

#include <QMessageBox>

#include "Settings.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::SettingsDialog) {
  ui->setupUi(this);
  ui->chordPreviewCheck->setChecked(ChordPreview::get());
  ui->customStyleCheck->setChecked(CustomStyle::get());
  ui->pauseReseekCheck->setChecked(SpacebarStop::get());
  ui->velocityDragCheck->setChecked(VelocityDrag::get());
  ui->swapScrollOrientationCheck->setChecked(SwapScrollOrientation::get());
  ui->swapZoomOrientationCheck->setChecked(SwapZoomOrientation::get());

  connect(ui->customStyleCheck, &QCheckBox::toggled, [this](bool) {
    QMessageBox::information(
        this, tr("Style change"),
        tr("Style change will take effect after program restart."));
  });
  connect(this, &QDialog::accepted, this, &SettingsDialog::apply);
}

void SettingsDialog::apply() {
  ChordPreview::set(ui->chordPreviewCheck->isChecked());
  CustomStyle::set(ui->customStyleCheck->isChecked());
  SpacebarStop::set(ui->pauseReseekCheck->isChecked());
  VelocityDrag::set(ui->velocityDragCheck->isChecked());
  SwapScrollOrientation::set(ui->swapScrollOrientationCheck->isChecked());
  SwapZoomOrientation::set(ui->swapZoomOrientationCheck->isChecked());
}

SettingsDialog::~SettingsDialog() { delete ui; }
