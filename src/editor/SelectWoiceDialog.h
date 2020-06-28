#ifndef SELECTWOICEDIALOG_H
#define SELECTWOICEDIALOG_H

#include <QDialog>

namespace Ui {
class SelectWoiceDialog;
}

class SelectWoiceDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SelectWoiceDialog(QWidget *parent = nullptr);
  void setWoices(QStringList woices);
  int getSelectedWoiceIndex();
  QString getUnitNameSelection();
  ~SelectWoiceDialog();
  // TODO: Probably also trigger audio preview on selection. Or just change it
  // so that this just prompts for a unit name for the current selected woice
  // instead of opening up a whole other dialog.

 private:
  Ui::SelectWoiceDialog *ui;
};

#endif  // SELECTWOICEDIALOG_H
