#ifndef NEWWOICEDIALOG_H
#define NEWWOICEDIALOG_H

#include <QDialog>
#include <QFileDialog>

namespace Ui {
class NewWoiceDialog;
}

class NewWoiceDialog : public QDialog {
  Q_OBJECT

 public:
  explicit NewWoiceDialog(QWidget *parent = nullptr);
  ~NewWoiceDialog();

 private:
  void buildFileList();
  void search();
  QString m_last_search_dir;
  QStringList m_last_search_files;
  QFileDialog *m_browse_search_folder_dialog;
  Ui::NewWoiceDialog *ui;
};

#endif  // NEWWOICEDIALOG_H
