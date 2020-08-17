#include "SelectWoiceDialog.h"

#include "ui_SelectWoiceDialog.h"

SelectWoiceDialog::SelectWoiceDialog(QAbstractListModel *model, QWidget *parent)
    : QDialog(parent), ui(new Ui::SelectWoiceDialog), m_model(model) {
  ui->setupUi(this);
  ui->woiceList->setModel(model);
  connect(
      ui->woiceList->selectionModel(), &QItemSelectionModel::currentRowChanged,
      [this](const QModelIndex &index) {
        QString name = ui->unitName->text();
        if (name.startsWith("u-") || name == "")
          ui->unitName->setText(QString("u-%1").arg(index.data().toString()));
      });

  connect(ui->woiceList, &QListView::activated,
          [this](const QModelIndex &) { accept(); });
}

int SelectWoiceDialog::getSelectedWoiceIndex() {
  return ui->woiceList->currentIndex().row();
}

QString SelectWoiceDialog::getUnitNameSelection() {
  return ui->unitName->text();
}
SelectWoiceDialog::~SelectWoiceDialog() { delete ui; }
