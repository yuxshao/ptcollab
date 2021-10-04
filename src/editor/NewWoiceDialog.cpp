#include "NewWoiceDialog.h"

#include <QDebug>
#include <QDirIterator>
#include <QTimer>

#include "ui_NewWoiceDialog.h"

const QStringList &woiceFilter() {
  static QStringList filter{"ptvoice", "ptnoise", "wav", "ogg"};
  return filter;
}

struct SearchEntry {
  QString relativePath;
  QString fullPath;
  QString displayString;
};

void NewWoiceDialog::buildFileListAsync() {
  QString dir = QFileInfo(ui->searchFolderLine->text()).absoluteFilePath();
  if (dir == "") return;
  if (dir != m_last_search_dir) {
    m_last_search_dir = dir;
    m_last_search_files.clear();
    m_last_search_dir_it = std::make_unique<QDirIterator>(
        dir, QDir::Files, QDirIterator::Subdirectories);
    m_last_search_num_files = 0;
  }
  if (!m_last_search_dir_it) return;
  const QStringList &filters = woiceFilter();
  for (int i = 0; i < 1000 && m_last_search_dir_it->hasNext(); ++i) {
    ++m_last_search_num_files;
    QFileInfo entry(m_last_search_dir_it->next());
    if (filters.contains(entry.suffix()))
      m_last_search_files.push_back(entry.absoluteFilePath());
  }

  ui->searchResultsList->clear();
  ui->searchResultsList->addItem(
      QString("Building list of ... found %1 out of %2 files so far")
          .arg(m_last_search_files.length())
          .arg(m_last_search_num_files));
  if (m_last_search_dir_it->hasNext())
    QTimer::singleShot(0, this, &NewWoiceDialog::buildFileListAsync);
  else {
    m_last_search_dir_it = nullptr;
    search();
  }
}

void NewWoiceDialog::search() {
  QString dir = QFileInfo(ui->searchFolderLine->text()).absoluteFilePath();
  if (dir != m_last_search_dir) {
    buildFileListAsync();
    return;
  }

  std::list<QStringMatcher> queries;
  for (const QString &query : ui->searchQueryLine->text().split(" "))
    queries.push_back(QStringMatcher(query, Qt::CaseInsensitive));

  ui->searchResultsList->clear();
  for (const QString &fullpath : m_last_search_files) {
    QFileInfo entry(fullpath);
    QString path = fullpath;
    path.remove(0, dir.length());
    bool matches = true;
    for (const QStringMatcher &query : queries)
      if (query.indexIn(path) == -1) {
        matches = false;
        break;
      }
    if (matches) {
      QListWidgetItem *item = new QListWidgetItem(
          entry.fileName() + " (" + QFileInfo(path).filePath() + ")",
          ui->searchResultsList);
      item->setData(Qt::UserRole, fullpath);
    }
  }
}

NewWoiceDialog::NewWoiceDialog(QWidget *parent)
    : QDialog(parent),
      m_browse_search_folder_dialog(
          new QFileDialog(this, "Select search base folder", "")),
      m_last_search_dir_it(nullptr),
      ui(new Ui::NewWoiceDialog) {
  ui->setupUi(this);
  m_browse_search_folder_dialog->setFileMode(QFileDialog::Directory);
  m_browse_search_folder_dialog->setOption(QFileDialog::ShowDirsOnly);
  connect(ui->searchFolderBrowseBtn, &QPushButton::clicked, this,
          [this]() { m_browse_search_folder_dialog->show(); });
  connect(m_browse_search_folder_dialog, &QFileDialog::accepted, this,
          [this]() {
            QStringList files = m_browse_search_folder_dialog->selectedFiles();
            if (files.length() > 0) ui->searchFolderLine->setText(files[0]);
          });

  connect(ui->searchBtn, &QPushButton::clicked, this, &NewWoiceDialog::search);
  connect(ui->searchQueryLine, &QLineEdit::textEdited, this, [this]() {
    if (ui->searchOnTypeCheck->isChecked()) search();
  });
}

NewWoiceDialog::~NewWoiceDialog() { delete ui; }
