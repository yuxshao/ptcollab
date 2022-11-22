#include "SelectWoiceDialog.h"

#include "ui_SelectWoiceDialog.h"

SelectWoiceDialog::SelectWoiceDialog(QAbstractListModel *model,
                                     PxtoneClient *client, QWidget *parent)
    : QDialog(parent),
      m_client(client),
      ui(new Ui::SelectWoiceDialog),
      m_model(model) {
  ui->setupUi(this);
  ui->woiceList->setModel(model);
  connect(
      ui->woiceList->selectionModel(), &QItemSelectionModel::currentRowChanged,
      [this](const QModelIndex &index) {
        int idx = index.row();
        if (idx >= 0)
          m_note_preview = std::make_unique<NotePreview>(
              m_client->pxtn(), &m_client->moo()->params,
              m_client->editState().mouse_edit_state.last_pitch,
              m_client->editState().mouse_edit_state.base_velocity, 48000,
              m_client->pxtn()->Woice_Get(idx), m_client->bufferSize(), this);
        else
          m_note_preview = nullptr;
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
