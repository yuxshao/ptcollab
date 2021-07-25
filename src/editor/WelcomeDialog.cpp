#include "WelcomeDialog.h"

#include "Settings.h"
#include "ui_WelcomeDialog.h"

WelcomeDialog::WelcomeDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::WelcomeDialog) {
  ui->setupUi(this);
  ui->labelVersion->setText(qApp->applicationName() + " " +
                            qApp->applicationVersion());
  ui->showAgainCheck->setChecked(Settings::ShowWelcomeDialog::get());
  QString connectToolTip = tr("Connect to an existing server.");
  QString newToolTip = tr("Create a new project without hosting.");
  QString openToolTip = tr("Open an existing project for offline use.");

  ui->buttonConnect->setToolTip(connectToolTip);
  ui->buttonNew->setToolTip(newToolTip);
  ui->buttonOpen->setToolTip(openToolTip);

  connect(ui->buttonNew, &QPushButton::released, this,
          &WelcomeDialog::buttonNewPressed);
  connect(ui->buttonOpen, &QPushButton::released, this,
          &WelcomeDialog::buttonOpenPressed);
  connect(ui->buttonConnect, &QPushButton::released, this,
          &WelcomeDialog::buttonConnectPressed);
}

void WelcomeDialog::closeEvent(QCloseEvent *) {
  Settings::ShowWelcomeDialog::set(ui->showAgainCheck->isChecked());
}

void WelcomeDialog::buttonNewPressed() {
  this->close();
  emit newSelected();
}
void WelcomeDialog::buttonOpenPressed() {
  this->close();
  emit openSelected();
}
void WelcomeDialog::buttonConnectPressed() {
  this->close();
  emit connectSelected();
}

WelcomeDialog::~WelcomeDialog() { delete ui; }
