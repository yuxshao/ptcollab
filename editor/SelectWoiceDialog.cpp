#include "SelectWoiceDialog.h"

#include "ui_SelectWoiceDialog.h"

SelectWoiceDialog::SelectWoiceDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::SelectWoiceDialog) {
  ui->setupUi(this);
}

void SelectWoiceDialog::setWoices(QStringList woices) {
  ui->woiceList->clear();
  ui->woiceList->addItems(woices);
}

int SelectWoiceDialog::getSelectedWoiceIndex() {
  return ui->woiceList->currentRow();
}

QString SelectWoiceDialog::getUnitNameSelection() {
  return ui->unitName->text();
}
SelectWoiceDialog::~SelectWoiceDialog() { delete ui; }
