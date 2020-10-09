#include "HostDialog.h"

#include <QFileDialog>
#include <QIntValidator>
#include <QSettings>

#include "FileSettings.h"
#include "ui_HostDialog.h"

HostDialog::HostDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::HostDialog) {
  ui->setupUi(this);

  QSettings settings;
  ui->usernameInput->setText(
      settings.value(DISPLAY_NAME_KEY, "Anonymous").toString());
  ui->portInput->setText(
      settings.value(HOST_SERVER_PORT_KEY, DEFAULT_PORT).toString());
  ui->portInput->setValidator(new QIntValidator(0, 65535, this));
  ui->saveRecordingGroup->setChecked(
      settings.value(SAVE_RECORDING_ENABLED_KEY, false).toBool());
  ui->hostGroup->setChecked(
      settings.value(HOSTING_ENABLED_KEY, false).toBool());

  connect(ui->openProjectGroup, &QGroupBox::toggled, [this](bool on) {
    ui->openProjectBtn->setEnabled(on);
    ui->openProjectFile->setEnabled(on);
  });
  connect(ui->openProjectBtn, &QPushButton::pressed, this,
          &HostDialog::selectProjectFile);

  connect(ui->saveRecordingGroup, &QGroupBox::toggled, [this](bool on) {
    ui->saveRecordingBtn->setEnabled(on);
    ui->saveRecordingFile->setEnabled(on);
  });
  connect(ui->saveRecordingBtn, &QPushButton::pressed, [this]() {
    QSettings settings;
    QString filename = QFileDialog::getSaveFileName(
        this, "Save file", settings.value(PTREC_SAVE_DIR_KEY).toString(),
        "ptcollab recordings (*.ptrec)");
    if (filename.isEmpty()) return;

    if (QFileInfo(filename).suffix() != "ptrec") filename += ".ptrec";
    // TODO: Save on accept
    // settings.setValue(PTREC_SAVE_DIR_KEY,
    // QFileInfo(filename).absolutePath());
    ui->saveRecordingFile->setText(filename);
  });

  connect(ui->hostGroup, &QGroupBox::toggled, [this](bool on) {
    ui->portInput->setEnabled(on);
    ui->usernameInput->setEnabled(on);
  });
}

bool HostDialog::selectProjectFile() {
  QSettings settings;
  QString dir = settings.value(PTCOP_DIR_KEY).toString();
  // ui->openProjectFile->setText(dir);
  QString filename = QFileDialog::getOpenFileName(
      this, "Open file", dir,
      "pxtone projects (*.ptcop);;ptcollab recordings (*.ptrec)");
  if (filename.isEmpty()) return false;
  ui->openProjectFile->setText(filename);
  return true;
}

HostDialog::~HostDialog() { delete ui; }

std::optional<QString> HostDialog::projectName() {
  if (ui->openProjectGroup->isChecked()) return ui->openProjectFile->text();
  return std::nullopt;
}

std::optional<QString> HostDialog::recordingName() {
  if (ui->saveRecordingGroup->isChecked()) return ui->saveRecordingFile->text();
  return std::nullopt;
}

QString HostDialog::username() { return ui->usernameInput->text(); }

std::optional<QString> HostDialog::port() {
  if (ui->hostGroup->isChecked()) return ui->portInput->text();
  return std::nullopt;
}

int HostDialog::start(bool open_file) {
  ui->openProjectGroup->setChecked(open_file);
  if (open_file) {
    if (!selectProjectFile()) return QDialog::Rejected;
  } else
    ui->openProjectFile->setText("");
  return QDialog::exec();
}

void HostDialog::persistSettings() {
  QSettings settings;

  std::optional<QString> filename = projectName();
  if (filename.has_value())
    settings.setValue(PTCOP_DIR_KEY,
                      QFileInfo(filename.value()).absolutePath());

  filename = recordingName();
  if (filename.has_value())
    settings.setValue(PTREC_SAVE_DIR_KEY,
                      QFileInfo(filename.value()).absolutePath());

  if (port().has_value()) {
    bool ok;
    int p = port().value().toInt(&ok);
    if (ok) settings.setValue(HOST_SERVER_PORT_KEY, p);
  }

  settings.setValue(SAVE_RECORDING_ENABLED_KEY,
                    ui->saveRecordingGroup->isChecked());
  settings.setValue(HOSTING_ENABLED_KEY, ui->hostGroup->isChecked());
  settings.setValue(DISPLAY_NAME_KEY, username());
}
