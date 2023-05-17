#include "HostDialog.h"

#include <QDebug>
#include <QFileDialog>
#include <QIntValidator>
#include <QSettings>

#include "Settings.h"
#include "ui_HostDialog.h"

HostDialog::HostDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::HostDialog) {
  ui->setupUi(this);

  connect(ui->openProjectGroup, &QGroupBox::toggled, [this](bool on) {
    if (!on) {
      m_last_project_file = ui->openProjectFile->text();
      ui->openProjectFile->setText(tr("New Project"));
    } else
      ui->openProjectFile->setText(m_last_project_file);
    ui->openProjectBtn->setEnabled(on);
    ui->openProjectFile->setEnabled(on);
  });
  connect(ui->openProjectBtn, &QPushButton::pressed, this,
          &HostDialog::selectProjectFile);

  connect(ui->saveRecordingGroup, &QGroupBox::toggled, [this](bool on) {
    if (!on) {
      m_last_record_file = ui->saveRecordingFile->text();
      ui->saveRecordingFile->setText(tr("No Recording"));
    } else
      ui->saveRecordingFile->setText(m_last_record_file);
    ui->saveRecordingBtn->setEnabled(on);
    ui->saveRecordingFile->setEnabled(on);
  });
  connect(ui->saveRecordingBtn, &QPushButton::pressed, [this]() {
    QSettings settings;
    QString filename = QFileDialog::getSaveFileName(
        this, "Save file", settings.value(PTREC_SAVE_FILE_KEY).toString(),
        "ptcollab recordings (*.ptrec)");
    if (filename.isEmpty()) return;

    if (QFileInfo(filename).suffix() != "ptrec") filename += ".ptrec";
    ui->saveRecordingFile->setText(filename);
  });

  connect(ui->hostGroup, &QGroupBox::toggled, [this](bool on) {
    ui->portInput->setEnabled(on);
    ui->usernameInput->setEnabled(on);
  });

  QSettings settings;
  ui->usernameInput->setText(
      settings.value(DISPLAY_NAME_KEY, "Anonymous").toString());
  ui->saveRecordingFile->setText(
      settings.value(PTREC_SAVE_FILE_KEY, "").toString());
  ui->portInput->setText(
      settings.value(HOST_SERVER_PORT_KEY, DEFAULT_PORT).toString());
  ui->portInput->setValidator(new QIntValidator(0, 65535, this));
  ui->saveRecordingGroup->setChecked(
      settings.value(SAVE_RECORDING_ENABLED_KEY, false).toBool());
  ui->hostGroup->setChecked(
      settings.value(HOSTING_ENABLED_KEY, false).toBool());
}

bool HostDialog::selectProjectFile() {
  QFileDialog f(this, "Open file", "",
                "pxtone projects (*.ptcop);;ptcollab recordings (*.ptrec)");
  f.setAcceptMode(QFileDialog::AcceptOpen);
  if (Settings::OpenProjectState::isSet()) {
    f.setDirectory("");  // Unset directory so the state also restores dir
    if (!f.restoreState(Settings::OpenProjectState::get()))
      qWarning() << "Failed to restore open dialog state";
  }
  QString filename;
  if (f.exec() == QDialog::Accepted) {
    filename = f.selectedFiles().value(0);
    Settings::OpenProjectState::set(f.saveState());
  }

  if (filename.isEmpty()) return false;
  ui->openProjectFile->setText(filename);
  return true;
}

HostDialog::~HostDialog() { delete ui; }

std::optional<QString> HostDialog::projectName() {
  if (ui->openProjectGroup->isChecked()) return ui->openProjectFile->text();
  return std::nullopt;
}

void HostDialog::setProjectName(std::optional<QString> name) {
  ui->openProjectGroup->setChecked(name.has_value());
  if (name.has_value()) ui->openProjectFile->setText(name.value());
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
  if (open_file)
    if (!selectProjectFile()) return QDialog::Rejected;

  return QDialog::exec();
}

void HostDialog::persistSettings() {
  QSettings settings;

  std::optional<QString> filename = projectName();
  if (filename.has_value())
    Settings::OpenProjectLastSelection::set(
        QFileInfo(filename.value()).absoluteFilePath());

  filename = recordingName();
  if (filename.has_value())
    settings.setValue(PTREC_SAVE_FILE_KEY,
                      QFileInfo(filename.value()).absoluteFilePath());

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
