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

extern AddWoice make_addWoice_from_path_exn(const QString &path,
                                            const QString &name);
class Query;

class NewWoiceDialog : public QDialog {
  Q_OBJECT

 public:
  explicit NewWoiceDialog(bool multi, const PxtoneClient *client,
                          QWidget *parent = nullptr);
  ~NewWoiceDialog();
  std::vector<AddWoice> selectedWoices();
  void inputMidi(const Input::Event::Event &);

 private:
  bool searchPart();
  void searchAsync();
  void previewWoice(const QString &path);
  void selectWoices(const QStringList &files);
  void setPreviewWoice(const QString &path);

  const PxtoneClient *m_client;
  QString m_last_search_dir;
  QStringList m_last_search_files;
  QFileDialog *m_browse_search_folder_dialog, *m_browse_woice_dialog;
  QStringList m_search_results_paths;
  std::unique_ptr<QDirIterator> m_last_search_dir_it;
  std::unique_ptr<std::list<Query>> m_queries;
  std::unique_ptr<NotePreview> m_note_preview;
  std::shared_ptr<pxtnWoice> m_preview_woice;
  std::map<int, std::unique_ptr<NotePreview>> m_record_note_preview;
  QStringList::iterator m_search_file_it;
  int m_last_search_num_files;
  Ui::NewWoiceDialog *ui;
};

#endif  // NEWWOICEDIALOG_H
