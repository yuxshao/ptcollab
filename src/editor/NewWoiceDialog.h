#ifndef NEWWOICEDIALOG_H
#define NEWWOICEDIALOG_H

#include <QDialog>
#include <QDirIterator>
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
  void buildFileListAsync();
  void search();
  QString m_last_search_dir;
  QStringList m_last_search_files;
  QFileDialog *m_browse_search_folder_dialog;
  std::unique_ptr<QDirIterator> m_last_search_dir_it;
  int m_last_search_num_files;
  Ui::NewWoiceDialog *ui;
};

#endif  // NEWWOICEDIALOG_H
