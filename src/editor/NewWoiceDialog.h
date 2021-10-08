#ifndef NEWWOICEDIALOG_H
#define NEWWOICEDIALOG_H

#include <QDialog>
#include <QDirIterator>
#include <QFileDialog>

#include "PxtoneClient.h"
#include "audio/NotePreview.h"

namespace Ui {
class NewWoiceDialog;
}

class Query;

class NewWoiceDialog : public QDialog {
  Q_OBJECT

 public:
  explicit NewWoiceDialog(bool multi, const PxtoneClient *client,
                          QWidget *parent = nullptr);
  ~NewWoiceDialog();
  std::vector<std::pair<QString, QString>> selectedWoices();

 private:
  bool searchPart();
  void searchAsync();
  void previewWoice(const QString &path);
  void selectWoices(const QStringList &files);

  const PxtoneClient *m_client;
  QString m_last_search_dir;
  QStringList m_last_search_files;
  QFileDialog *m_browse_search_folder_dialog, *m_browse_woice_dialog;
  QStringList m_search_results_paths;
  std::unique_ptr<QDirIterator> m_last_search_dir_it;
  std::unique_ptr<std::list<Query>> m_queries;
  std::unique_ptr<NotePreview> m_note_preview;
  QStringList::iterator m_search_file_it;
  int m_last_search_num_files;
  Ui::NewWoiceDialog *ui;
};

#endif  // NEWWOICEDIALOG_H
