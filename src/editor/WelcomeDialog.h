#ifndef WELCOMEDIALOG_H
#define WELCOMEDIALOG_H

#include <QDialog>

namespace Ui {
class WelcomeDialog;
}

class WelcomeDialog : public QDialog {
  Q_OBJECT

 public:
  explicit WelcomeDialog(QWidget *parent = nullptr);
  ~WelcomeDialog();

 signals:
  void newSelected();
  void openSelected();
  void connectSelected();

 protected slots:
  void buttonNewPressed();
  void buttonOpenPressed();
  void buttonConnectPressed();

 private:
  Ui::WelcomeDialog *ui;
};

#endif  // WELCOMEDIALOG_H
