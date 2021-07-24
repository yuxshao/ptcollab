#include "WelcomeDialog.h"
#include "ui_WelcomeDialog.h"
#include "Settings.h"

WelcomeDialog::WelcomeDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::WelcomeDialog)
{
  ui->setupUi(this);
  ui->labelVersion->setText(qApp->applicationName() + " " + qApp->applicationVersion());
  ui->showAgainCheck->setChecked(Settings::ShowLandingPage::get());     //Shouldn't be necessary but if it is somehow false i want the checkbox to be right

  QString connectToolTip =      tr("Connect to an existing server.");
  QString newToolTip =          tr("Create a new project without hosting.");
  QString openToolTip =         tr("Open an existing project for offline use.");

  ui->buttonConnect->setToolTip(connectToolTip);
  ui->buttonNew->setToolTip(newToolTip);
  ui->buttonOpen->setToolTip(openToolTip);

  connect(ui->buttonNew, SIGNAL(released()), this, SLOT(buttonNewPressed()));
  connect(ui->buttonOpen, SIGNAL(released()), this, SLOT(buttonOpenPressed()));
  connect(ui->buttonConnect, SIGNAL(released()), this, SLOT(buttonConnectPressed()));
  this->setFixedSize(QSize());
}

void WelcomeDialog::closeEvent(QCloseEvent *)
{
  Settings::ShowLandingPage::set(ui->showAgainCheck->isChecked());
}

WelcomeDialog::~WelcomeDialog()
{
  delete ui;
}
