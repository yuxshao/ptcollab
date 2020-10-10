#ifndef HOSTDIALOG_H
#define HOSTDIALOG_H

#include <optional>
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
  QString m_last_project_file, m_last_record_file;
  Ui::HostDialog *ui;
  bool selectProjectFile();
};

#endif  // HOSTDIALOG_H
