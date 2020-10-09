#ifndef HOSTDIALOG_H
#define HOSTDIALOG_H

#include <QDialog>

namespace Ui {
class HostDialog;
}

class HostDialog : public QDialog {
  Q_OBJECT

 public:
  explicit HostDialog(QWidget *parent = nullptr);
  ~HostDialog();
  void openFile();
  std::optional<QString> projectName();
  std::optional<QString> recordingName();
  QString username();
  std::optional<QString> port();
  int start(bool open_file);
  void persistSettings();

  // TODO: Maybe validate here

 private:
  Ui::HostDialog *ui;
  void selectProjectFile();
};

#endif  // HOSTDIALOG_H
