#include "ShortcutsDialog.h"

#include "ui_ShortcutsDialog.h"

ShortcutsDialog::ShortcutsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ShortcutsDialog) {
  ui->setupUi(this);
  ui->tableWidget->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
}

ShortcutsDialog::~ShortcutsDialog() { delete ui; }
