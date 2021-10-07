#include "NewWoiceDialog.h"

#include <QDebug>
#include <QDirIterator>
#include <QTimer>

#include "Settings.h"
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

bool NewWoiceDialog::searchPart() {
  QString dir = QFileInfo(ui->searchFolderLine->text()).absoluteFilePath();
  if (dir == "") return true;

  if (dir != m_last_search_dir) {
    if (QDir(dir).exists()) {
      m_browse_search_folder_dialog->setDirectory(dir);
      Settings::SearchWoiceState::set(
          m_browse_search_folder_dialog->saveState());
    }
    m_queries = nullptr;
    m_last_search_dir = dir;
    m_last_search_files.clear();
    m_last_search_dir_it = std::make_unique<QDirIterator>(
        dir, QDir::Files, QDirIterator::Subdirectories);
    m_last_search_num_files = 0;
    ui->searchResultsList->clear();
    return false;
  }

  if (m_last_search_dir_it) {
    const QStringList &filters = woiceFilter();
    for (int i = 0; i < 1000 && m_last_search_dir_it->hasNext(); ++i) {
      ++m_last_search_num_files;
      QFileInfo entry(m_last_search_dir_it->next());
      if (filters.contains(entry.suffix()))
        m_last_search_files.push_back(entry.absoluteFilePath());
    }

    ui->searchStatusLabel->setText(
        QString("Scanning...\n(%1 voices / %2 files)")
            .arg(m_last_search_files.length())
            .arg(m_last_search_num_files));
    if (!m_last_search_dir_it->hasNext()) {
      m_last_search_dir_it = nullptr;

      ui->searchResultsList->clear();
      m_search_file_it = m_last_search_files.begin();
    }
    return false;
  }

  if (m_queries == nullptr) {
    m_queries = std::make_unique<std::list<QStringMatcher>>();
    for (const QString &query : ui->searchQueryLine->text().split(" "))
      m_queries->push_back(QStringMatcher(query, Qt::CaseInsensitive));

    ui->searchResultsList->clear();
    m_search_file_it = m_last_search_files.begin();
  }

  for (int i = 0; i < 100 && m_search_file_it != m_last_search_files.end();
       ++i, ++m_search_file_it) {
    QFileInfo entry(*m_search_file_it);
    QString path = *m_search_file_it;
    path.remove(0, dir.length());
    bool matches = true;
    for (const QStringMatcher &query : *m_queries)
      if (query.indexIn(path) == -1) {
        matches = false;
        break;
      }
    if (matches) {
      QListWidgetItem *item = new QListWidgetItem(
          entry.fileName() + " (" + QFileInfo(path).filePath() + ")",
          ui->searchResultsList);
      item->setData(Qt::UserRole, *m_search_file_it);
    }
  }

  if (m_search_file_it != m_last_search_files.end()) {
    ui->searchStatusLabel->setText(
        QString("Filtering...\n(%1 matches / %2 voices)")
            .arg(ui->searchResultsList->count())
            .arg(m_last_search_files.size()));
    return false;
  }

  ui->searchStatusLabel->setText(QString("%1 matches / %2 voices")
                                     .arg(ui->searchResultsList->count())
                                     .arg(m_last_search_files.size()));
  return true;
}

void NewWoiceDialog::searchAsync() {
  if (!searchPart() && isVisible())
    QTimer::singleShot(0, this, &NewWoiceDialog::searchAsync);
}

void NewWoiceDialog::selectWoices(const QStringList &files) {
  QStringList names;
  for (const auto &file : files) names.push_back(QFileInfo(file).baseName());

  ui->voiceNameLine->setText(names.join(";"));
  ui->voicePathLine->setText(files.join(";"));
}

NewWoiceDialog::NewWoiceDialog(bool multi, QWidget *parent)
    : QDialog(parent),
      m_browse_search_folder_dialog(
          new QFileDialog(this, "Select search base folder", "")),
      m_browse_woice_dialog(new QFileDialog(this, "Select voice", "")),
      m_last_search_dir_it(nullptr),
      ui(new Ui::NewWoiceDialog) {
  ui->setupUi(this);

  m_browse_woice_dialog->setDirectory(QString());
  if (!m_browse_woice_dialog->restoreState(Settings::BrowseWoiceState::get()))
    qDebug() << "Could not restore browse woice dialog state";
  m_browse_woice_dialog->setFileMode(multi ? QFileDialog::ExistingFiles
                                           : QFileDialog::ExistingFile);
  m_browse_woice_dialog->setNameFilter(
      tr("Instruments (*.ptvoice *.ptnoise *.wav *.ogg)"));

  connect(ui->voicePathBrowseBtn, &QPushButton::clicked, this,
          [this]() { m_browse_woice_dialog->show(); });
  connect(m_browse_woice_dialog, &QFileDialog::accepted, this, [this]() {
    Settings::BrowseWoiceState::set(m_browse_woice_dialog->saveState());
    selectWoices(m_browse_woice_dialog->selectedFiles());
  });

  m_browse_search_folder_dialog->setDirectory(QString());
  if (!m_browse_search_folder_dialog->restoreState(
          Settings::SearchWoiceState::get()))
    qDebug() << "Could not restore woice search dialog state";
  m_browse_search_folder_dialog->setFileMode(QFileDialog::Directory);
  m_browse_search_folder_dialog->setOption(QFileDialog::ShowDirsOnly);
  QStringList files = m_browse_search_folder_dialog->selectedFiles();
  if (files.length() > 0) ui->searchFolderLine->setText(files[0]);

  ui->searchResultsList->setSelectionMode(
      multi ? QAbstractItemView::ExtendedSelection
            : QAbstractItemView::SingleSelection);
  connect(
      ui->searchResultsList, &QListWidget::itemSelectionChanged, this,
      [this]() {
        QStringList paths;
        for (QListWidgetItem *item : ui->searchResultsList->selectedItems()) {
          paths.push_back(item->data(Qt::UserRole).toString());
        }
        selectWoices(paths);
      });

  ui->searchOnTypeCheck->setChecked(Settings::SearchOnType::get());
  connect(ui->searchOnTypeCheck, &QCheckBox::stateChanged, this, [this](int) {
    Settings::SearchOnType::set(ui->searchOnTypeCheck->isChecked());
  });

  connect(ui->searchFolderBrowseBtn, &QPushButton::clicked, this,
          [this]() { m_browse_search_folder_dialog->show(); });
  connect(m_browse_search_folder_dialog, &QFileDialog::accepted, this,
          [this]() {
            Settings::SearchWoiceState::set(
                m_browse_search_folder_dialog->saveState());
            QStringList files = m_browse_search_folder_dialog->selectedFiles();
            if (files.length() > 0) ui->searchFolderLine->setText(files[0]);
          });

  connect(ui->searchBtn, &QPushButton::clicked, this,
          [this]() { searchAsync(); });
  connect(ui->searchQueryLine, &QLineEdit::textEdited, this, [this]() {
    m_queries = nullptr;
    if (ui->searchOnTypeCheck->isChecked()) searchAsync();
  });
}

std::vector<std::pair<QString, QString>> NewWoiceDialog::selectedWoices() {
  std::vector<std::pair<QString, QString>> woices;
  QStringList names = ui->voiceNameLine->text().split(";");
  QStringList paths = ui->voicePathLine->text().split(";");
  for (int i = 0; i < paths.length(); ++i) {
    QString name;
    if (i >= names.length())
      name = QFileInfo(paths[i]).baseName();
    else
      name = names[i];
    woices.push_back({paths[i], name});
  }
  return woices;
}

NewWoiceDialog::~NewWoiceDialog() { delete ui; }
