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

class Query {
 public:
  Query(const QString &q) : negated(false) {
    if (q.length() == 0) return;
    if (q[0] == "-") {
      negated = true;
      matcher = QStringMatcher(QString(q).remove(0, 1), Qt::CaseInsensitive);
    } else {
      negated = false;
      matcher = QStringMatcher(q, Qt::CaseInsensitive);
    }
  }

  bool matches(const QString &path) const {
    return (matcher.indexIn(path) == -1) == negated;
  }

 private:
  bool negated;
  QStringMatcher matcher;
};

bool NewWoiceDialog::searchPart() {
  QString dir = QFileInfo(ui->searchFolderLine->text()).absoluteFilePath();
  if (dir == "") return true;

  if (dir != m_last_search_dir) {
    // I'm told that in the past setDirectory would cause crashes on Mac after
    // the first set. so I'm holding off on this for now.
    /*if (QDir(dir).exists()) {
      m_browse_search_folder_dialog->setDirectory(dir);
      Settings::SearchWoiceState::set(
          m_browse_search_folder_dialog->saveState());
    }*/
    m_queries = nullptr;
    m_last_search_dir = dir;
    m_last_search_files.clear();
    m_last_search_dir_it = std::make_unique<QDirIterator>(
        dir, QDir::Files, QDirIterator::Subdirectories);
    m_last_search_num_files = 0;
    ui->searchResultsList->clear();
    m_search_results_paths.clear();
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
      m_search_results_paths.clear();
      m_search_file_it = m_last_search_files.begin();
    }
    return false;
  }

  if (m_queries == nullptr) {
    m_queries = std::make_unique<std::list<Query>>();
    for (const QString &query : ui->searchQueryLine->text().split(" "))
      m_queries->emplace_back(query);

    ui->searchResultsList->clear();
    m_search_results_paths.clear();
    m_search_file_it = m_last_search_files.begin();
  }

  QStringList labels;
  for (int i = 0; i < 50 && m_search_file_it != m_last_search_files.end();
       ++i, ++m_search_file_it) {
    QFileInfo entry(*m_search_file_it);
    QString path = *m_search_file_it;
    path.remove(0, dir.length());
    bool matches = true;
    for (const Query &query : *m_queries)
      if (!query.matches(path)) {
        matches = false;
        break;
      }
    if (matches) {
      m_search_results_paths.push_back(*m_search_file_it);
      labels.push_back(entry.fileName() + " (" + QFileInfo(path).filePath() +
                       ")");
    }
  }
  ui->searchResultsList->addItems(labels);

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

AddWoice make_addWoice_from_path(const QString &path, const QString &name) {
  QFileInfo fileinfo(path);
  QString filename = fileinfo.fileName();
  QString suffix = fileinfo.suffix().toLower();
  pxtnWOICETYPE type;

  if (suffix == "ptvoice")
    type = pxtnWOICE_PTV;
  else if (suffix == "ptnoise")
    type = pxtnWOICE_PTN;
  else if (suffix == "ogg" || suffix == "oga")
    type = pxtnWOICE_OGGV;
  else if (suffix == "wav")
    type = pxtnWOICE_PCM;
  else {
    throw QString("Voice file (%1) has invalid extension (%2)")
        .arg(filename)
        .arg(suffix);
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    throw QString("Could not open file (%1)").arg(filename);

  return AddWoice{type, name == "" ? fileinfo.baseName() : name,
                  file.readAll()};
}

void NewWoiceDialog::setPreviewWoice(const QString &path) {
  AddWoice a(make_addWoice_from_path(path, ""));
  m_preview_woice = std::make_shared<pxtnWoice>();

  pxtnDescriptor d;
  d.set_memory_r(a.data.constData(), a.data.size());
  pxtnERR result = m_preview_woice->read(&d, a.type);
  if (result != pxtnOK) {
    qDebug() << "Could not load preview woice at path" << path;
    m_preview_woice = nullptr;
  } else
    m_client->pxtn()->Woice_ReadyTone(m_preview_woice);
}
void NewWoiceDialog::previewWoice(const QString &path) {
  setPreviewWoice(path);
  if (m_preview_woice == nullptr) return;

  bool ok;
  int key = ui->previewKeyLine->text().toInt(&ok) * PITCH_PER_KEY;
  if (!ok) key = EVENTDEFAULT_KEY;

  int vel =
      ui->previewVolSlider->value() * 128 / ui->previewVolSlider->maximum();
  m_note_preview = std::make_unique<NotePreview>(
      m_client->pxtn(), &m_client->moo()->params, key, vel, 48000,
      m_preview_woice, m_client->audioState()->bufferSize(), this);
}

NewWoiceDialog::NewWoiceDialog(bool multi, const PxtoneClient *client,
                               QWidget *parent)
    : QDialog(parent),
      m_client(client),
      m_browse_search_folder_dialog(
          new QFileDialog(this, "Select search base folder", "")),
      m_browse_woice_dialog(new QFileDialog(this, "Select voice", "")),
      m_last_search_dir_it(nullptr),
      ui(new Ui::NewWoiceDialog) {
  ui->setupUi(this);
  ui->previewKeyLine->setText(
      QString("%1").arg(EVENTDEFAULT_KEY / PITCH_PER_KEY));
  ui->previewKeyLine->setValidator(new QIntValidator(0, 150, this));

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

  connect(m_browse_woice_dialog, &QFileDialog::currentChanged, this,
          &NewWoiceDialog::previewWoice);

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
  connect(ui->searchResultsList, &QListWidget::itemSelectionChanged, this,
          [this]() {
            QStringList paths;
            QModelIndexList rows(
                ui->searchResultsList->selectionModel()->selectedRows());
            for (const QModelIndex &i : rows)
              paths.push_back(m_search_results_paths[i.row()]);
            if (rows.count() > 0) {
              int row = ui->searchResultsList->currentIndex().row();
              previewWoice(m_search_results_paths[row]);
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

std::vector<AddWoice> NewWoiceDialog::selectedWoices() {
  std::vector<AddWoice> woices;
  QStringList names = ui->voiceNameLine->text().split(";");
  QStringList paths = ui->voicePathLine->text().split(";");
  for (int i = 0; i < paths.length(); ++i) {
    QString name;
    if (i < names.length()) name = names[i];
    woices.push_back(make_addWoice_from_path(paths[i], name));
  }
  return woices;
}

void NewWoiceDialog::inputMidi(const Input::Event::Event &e) {
  std::visit(overloaded{
                 [this](const Input::Event::On &e) {
                   if (m_preview_woice == nullptr) return;
                   m_record_note_preview[e.key] = std::make_unique<NotePreview>(
                       m_client->pxtn(), &m_client->moo()->params, e.key, e.vel,
                       100000000, m_preview_woice,
                       m_client->audioState()->bufferSize(), this);
                 },
                 [this](const Input::Event::Off &e) {
                   if (m_record_note_preview[e.key])
                     m_record_note_preview[e.key]->processEvent(EVENTKIND_ON,
                                                                0);
                 },
                 [](const Input::Event::Skip &) {},
             },
             e);
}

NewWoiceDialog::~NewWoiceDialog() { delete ui; }
