#ifndef SELECTWOICEDIALOG_H
#define SELECTWOICEDIALOG_H

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
  explicit SelectWoiceDialog(QWidget *parent = nullptr);
  void setWoices(QStringList woices);
  int getSelectedWoiceIndex();
  QString getUnitNameSelection();
  ~SelectWoiceDialog();

 private:
  Ui::SelectWoiceDialog *ui;
};

#endif  // SELECTWOICEDIALOG_H
