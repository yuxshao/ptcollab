#include "RenderDialog.h"

#include <QDoubleValidator>
#include <QFileDialog>

#include "Settings.h"
#include "ui_RenderDialog.h"

static QDoubleValidator lengthValidator(0, std::numeric_limits<double>::max(),
                                        2);
RenderDialog::RenderDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::RenderDialog) {
  ui->setupUi(this);
  ui->lengthEdit->setValidator(&lengthValidator);
  ui->fadeOutEdit->setValidator(&lengthValidator);
  ui->saveToEdit->setText(Settings::RenderFileDestination::get());

  connect(ui->saveToBtn, &QPushButton::pressed, [this]() {
    QString filename = QFileDialog::getSaveFileName(
        this, "Render to file", Settings::RenderFileDestination::get(),
        tr("WAV file (*.wav)"));
    if (QFileInfo(filename).suffix() != "wav") filename += ".wav";
    filename = QFileInfo(filename).absoluteFilePath();
    ui->saveToEdit->setText(filename);
  });

  connect(ui->saveToEdit, &QLineEdit::textChanged,
          &Settings::RenderFileDestination::set);
}

RenderDialog::~RenderDialog() { delete ui; }

static QString format_length(double l) {
  return QString("%1").arg(l, 0, 'f', 2);
}

void RenderDialog::setSongLength(double l) {
  ui->fullField->setText(format_length(l) + "s");
  ui->lengthEdit->setText(format_length(l));
  ui->fadeOutEdit->setText("0");
}

void RenderDialog::setVolume(double v) {
  ui->volumeEdit->setText(format_length(v));
}

void RenderDialog::setSongLoopLength(double l) {
  ui->loopField->setText(format_length(l) + "s");
}
double RenderDialog::renderLength() {
  bool ok;
  double v = ui->lengthEdit->text().toDouble(&ok);
  if (!ok) throw QString("Invalid render length");
  return v;
}

double RenderDialog::renderFadeout() {
  bool ok;
  double v = ui->fadeOutEdit->text().toDouble(&ok);
  if (!ok) throw QString("Invalid render fadeout length");
  return v;
}

double RenderDialog::renderVolume() {
  bool ok;
  double v = ui->volumeEdit->text().toDouble(&ok);
  if (!ok) throw QString("Invalid volume");
  return v;
}

QString RenderDialog::renderDestination() { return ui->saveToEdit->text(); }
