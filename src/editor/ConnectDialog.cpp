#include "ConnectDialog.h"
#include "ui_ConnectDialog.h"

ConnectDialog::ConnectDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::ConnectDialog)
{
  ui->setupUi(this);
}

ConnectDialog::~ConnectDialog()
{
  delete ui;
}
