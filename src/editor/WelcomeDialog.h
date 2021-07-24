#ifndef WELCOMEDIALOG_H
#define WELCOMEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QToolTip>
#include <QProxyStyle>

namespace Ui {
  class WelcomeDialog;
}

class WelcomeDialog : public QDialog
{
  Q_OBJECT

public:
  explicit WelcomeDialog(QWidget *parent = nullptr);
  ~WelcomeDialog();

signals:
  void newSelected();
  void openSelected();
  void connectSelected();

protected slots:
  void closeEvent(QCloseEvent *) override;
  void buttonNewPressed()
  {
    this->close();
    emit(newSelected());
  }
  void buttonOpenPressed()
  {
    this->close();
    emit(openSelected());
  }
  void buttonConnectPressed()
  {
    this->close();
    emit(connectSelected());
  }

private:
  Ui::WelcomeDialog *ui;
};

#endif // WELCOMEDIALOG_H
