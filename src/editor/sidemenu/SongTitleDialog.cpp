#include "SongTitleDialog.h"

#include <QRegularExpression>

#include "ui_SongTitleDialog.h"

SongTitleDialog::SongTitleDialog(const QString &title, const QString &comment,
                                 QWidget *parent)
    : QDialog(parent), ui(new Ui::SongTitleDialog) {
  ui->setupUi(this);
  ui->titleLineEdit->setText(title);
  ui->commentTextEdit->setText(comment);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
}

QString SongTitleDialog::title() { return ui->titleLineEdit->text(); }
QString SongTitleDialog::comment() {
  return ui->commentTextEdit->toPlainText().replace(
      QRegularExpression("[\r*\n]"), "\r\n");
}

SongTitleDialog::~SongTitleDialog() { delete ui; }
