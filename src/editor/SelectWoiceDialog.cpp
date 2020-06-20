#include "SelectWoiceDialog.h"

#include "ui_SelectWoiceDialog.h"

SelectWoiceDialog::SelectWoiceDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::SelectWoiceDialog) {
  ui->setupUi(this);
  connect(
      ui->woiceList, &QListWidget::currentTextChanged,
      [this](const QString &s) {
        if (ui->unitName->text().startsWith("u-") || ui->unitName->text() == "")
          ui->unitName->setText(QString("u-%1").arg(s));
      });

  connect(ui->woiceList, &QListWidget::itemActivated,
          [this](const QListWidgetItem *) { accept(); });
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
