#ifndef SELECTWOICEDIALOG_H
#define SELECTWOICEDIALOG_H

#include <QAbstractListModel>
#include <QDialog>

#include "editor/PxtoneClient.h"
#include "editor/audio/NotePreview.h"

namespace Ui {
class SelectWoiceDialog;
}

// Obsolete now that the main view includes a woice list.
// 2020-07-04 Update: Not obsolete now that woice list is not always visilbe
// when adding unit.
class SelectWoiceDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SelectWoiceDialog(QAbstractListModel *model, PxtoneClient *client,
                             QWidget *parent = nullptr);
  int getSelectedWoiceIndex();
  QString getUnitNameSelection();
  ~SelectWoiceDialog();

 private:
  PxtoneClient *m_client;
  std::unique_ptr<NotePreview> m_note_preview;
  Ui::SelectWoiceDialog *ui;
  QAbstractListModel *m_model;
};

#endif  // SELECTWOICEDIALOG_H
