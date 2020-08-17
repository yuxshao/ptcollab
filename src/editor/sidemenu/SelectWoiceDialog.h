#ifndef SELECTWOICEDIALOG_H
#define SELECTWOICEDIALOG_H

#include <QAbstractListModel>
#include <QDialog>

namespace Ui {
class SelectWoiceDialog;
}

// Obsolete now that the main view includes a woice list.
// 2020-07-04 Update: Not obsolete now that woice list is not always visilbe
// when adding unit.
class SelectWoiceDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SelectWoiceDialog(QAbstractListModel *model,
                             QWidget *parent = nullptr);
  int getSelectedWoiceIndex();
  QString getUnitNameSelection();
  ~SelectWoiceDialog();

 private:
  Ui::SelectWoiceDialog *ui;
  QAbstractListModel *m_model;
};

#endif  // SELECTWOICEDIALOG_H
