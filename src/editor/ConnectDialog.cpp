#include "ConnectDialog.h"

#include <QSettings>

#include "Settings.h"
#include "ui_ConnectDialog.h"

ConnectDialog::ConnectDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ConnectDialog) {
  ui->setupUi(this);
}

QString ConnectDialog::username() { return ui->usernameInput->text(); }

QString ConnectDialog::address() { return ui->addressInput->text(); }

const static QString DEFAULT_ADDRESS =
    QString("%1:%2").arg("localhost").arg(DEFAULT_PORT);

int ConnectDialog::exec() {
  QSettings settings;
  ui->addressInput->setText(
      settings.value(CONNECT_SERVER_KEY, DEFAULT_ADDRESS).toString());
  ui->usernameInput->setText(
      settings.value(DISPLAY_NAME_KEY, "Anonymous").toString());

  return QDialog::exec();
}

void ConnectDialog::persistSettings() {
  QSettings settings;
  settings.setValue(DISPLAY_NAME_KEY, username());
  settings.setValue(CONNECT_SERVER_KEY, address());
}

ConnectDialog::~ConnectDialog() { delete ui; }
