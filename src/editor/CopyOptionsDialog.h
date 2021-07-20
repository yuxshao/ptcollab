#ifndef COPYOPTIONSDIALOG_H
#define COPYOPTIONSDIALOG_H

#include <QCheckBox>
#include <QDialog>

#include "Clipboard.h"

namespace Ui {
class CopyOptionsDialog;
}

class CopyOptionsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit CopyOptionsDialog(Clipboard *, QWidget *parent = nullptr);
  ~CopyOptionsDialog();

  void setCopyOptions();
  void keyPressEvent(QKeyEvent *event) override;
 signals:
  void optionChanged(EVENTKIND, bool);

 private:
  Clipboard *m_clipboard;
  std::map<EVENTKIND, QCheckBox *> mapping;
  Ui::CopyOptionsDialog *ui;
};

#endif  // COPYOPTIONSDIALOG_H
