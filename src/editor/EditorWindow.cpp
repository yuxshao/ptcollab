#include "EditorWindow.h"

#include <QDebug>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QSplitter>
#include <QtMultimedia/QAudioDeviceInfo>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>

#include "AudioFormat.h"
#include "pxtone/pxtnDescriptor.h"
#include "server/BroadcastServer.h"
#include "server/Client.h"
#include "ui_EditorWindow.h"

// TODO: Maybe we could not hard-code this and change the engine to be dynamic
// w/ smart pointers.
static constexpr int EVENT_MAX = 1000000;

// TODO: Put this somewhere else, like in remote action or sth.
AddWoice make_addWoice_from_path(const QString &path) {
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

  QString name = fileinfo.baseName();
  return AddWoice{type, name, file.readAll()};
}

EditorWindow::EditorWindow(QWidget *parent)
    : QMainWindow(parent),
      m_pxtn_device(this, &m_pxtn),
      m_server(nullptr),
      m_client(new Client(this)),
      m_filename(""),
      m_server_status(new QLabel("Not hosting", this)),
      m_client_status(new QLabel("Not connected", this)),
      m_note_preview(nullptr),
      ui(new Ui::EditorWindow) {
  m_pxtn.init_collage(EVENT_MAX);
  int channel_num = 2;
  int sample_rate = 44100;
  m_pxtn.set_destination_quality(channel_num, sample_rate);
  ui->setupUi(this);
  resize(QDesktopWidget().availableGeometry(this).size() * 0.7);

  QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
  if (!info.isFormatSupported(pxtoneAudioFormat())) {
    qWarning()
        << "Raw audio format not supported by backend, cannot play audio.";
    return;
  }

  m_audio = new QAudioOutput(pxtoneAudioFormat(), this);

  // m_audio->setBufferSize(441000);

  // Apparently this reduces latency in pulseaudio, but also makes
  // some sounds choppier
  // m_audio->setCategory("game");
  m_audio->setVolume(1.0);

  m_splitter = new QSplitter(Qt::Horizontal, this);
  setCentralWidget(m_splitter);

  m_units = new UnitListModel(&m_pxtn, this);
  m_keyboard_editor = new KeyboardEditor(&m_pxtn, m_audio, m_client, m_units);
  connect(
      m_client, &Client::connected,
      [this](pxtnDescriptor &desc, QList<ServerAction> &history, qint64 uid) {
        HostAndPort host_and_port = m_client->currentlyConnectedTo();
        m_client_status->setText(tr("Connected to %1:%2")
                                     .arg(host_and_port.host)
                                     .arg(host_and_port.port));
        QMessageBox::information(this, "Connected", "Connected to server.");
        m_side_menu->setEditWidgetsEnabled(true);
        loadDescriptor(desc);
        m_keyboard_editor->setUid(uid);
        m_keyboard_editor->loadHistory(history);
      });
  connect(m_client, &Client::disconnected, [this]() {
    m_client_status->setText(tr("Not connected"));
    m_keyboard_editor->clearRemoteEditStates();
    QMessageBox::information(this, "Disconnected", "Disconnected from server.");
  });
  connect(m_client, &Client::errorOccurred, [this](QString error) {
    QMessageBox::information(this, "Connection error",
                             tr("Connection error: %1").arg(error));
  });
  statusBar()->addPermanentWidget(m_server_status);
  statusBar()->addPermanentWidget(m_client_status);

  m_scroll_area = new EditorScrollArea(m_splitter);
  m_scroll_area->setWidget(m_keyboard_editor);
  m_scroll_area->setBackgroundRole(QPalette::Dark);
  m_scroll_area->setVisible(true);

  m_side_menu = new SideMenu(m_units, this);

  m_side_menu->setEditWidgetsEnabled(false);
  m_splitter->addWidget(m_side_menu);
  m_splitter->addWidget(m_scroll_area);
  m_splitter->setSizes(QList{10, 10000});

  connect(m_side_menu, &SideMenu::quantXIndexUpdated, m_keyboard_editor,
          &KeyboardEditor::setQuantXIndex);
  connect(m_side_menu, &SideMenu::quantYIndexUpdated, m_keyboard_editor,
          &KeyboardEditor::setQuantYIndex);
  connect(m_keyboard_editor, &KeyboardEditor::quantXIndexChanged, m_side_menu,
          &SideMenu::setQuantXIndex);
  connect(m_side_menu, &SideMenu::quantYIndexUpdated, m_keyboard_editor,
          &KeyboardEditor::setQuantYIndex);

  connect(m_side_menu, &SideMenu::currentUnitChanged, m_keyboard_editor,
          &KeyboardEditor::setCurrentUnitNo);
  connect(m_keyboard_editor, &KeyboardEditor::currentUnitNoChanged, m_side_menu,
          &SideMenu::setCurrentUnit);
  connect(m_keyboard_editor, &KeyboardEditor::onEdit,
          [=]() { m_side_menu->setModified(true); });
  connect(m_keyboard_editor, &KeyboardEditor::userListChanged, m_side_menu,
          &SideMenu::setUserList);

  connect(
      m_side_menu, &SideMenu::addUnit, [this](int woice_id, QString unit_name) {
        m_client->sendAction(
            AddUnit{woice_id, m_pxtn.Woice_Get(woice_id)->get_name_buf(nullptr),
                    unit_name});
      });
  connect(m_side_menu, &SideMenu::addWoice, [this](QString path) {
    try {
      m_client->sendAction(make_addWoice_from_path(path));
    } catch (const QString &e) {
      QMessageBox::critical(this, tr("Unable to add voice"), e);
    }
  });

  connect(m_side_menu, &SideMenu::changeWoice,
          [this](int idx, QString name, QString path) {
            try {
              m_client->sendAction(ChangeWoice{RemoveWoice{idx, name},
                                               make_addWoice_from_path(path)});
            } catch (const QString &e) {
              QMessageBox::critical(this, tr("Unable to change voice"), e);
            }
          });

  connect(m_side_menu, &SideMenu::removeWoice, [this](int idx, QString name) {
    if (m_pxtn.Woice_Num() == 1) {
      QMessageBox::critical(this, tr("Error"), tr("Cannot remove last voice."));
      return;
    }
    if (idx >= 0) m_client->sendAction(RemoveWoice{idx, name});
  });
  connect(m_side_menu, &SideMenu::candidateWoiceSelected, [this](QString path) {
    try {
      AddWoice a(make_addWoice_from_path(path));
      std::shared_ptr<pxtnWoice> woice = std::make_shared<pxtnWoice>();
      {
        pxtnDescriptor d;
        d.set_memory_r(a.data.constData(), a.data.size());
        woice->read(&d, a.type);
        m_pxtn.Woice_ReadyTone(woice);
      }
      // TODO: stop existing note preview on creation of new one in this case
      m_note_preview = std::make_unique<NotePreview>(
          &m_pxtn, EVENTDEFAULT_KEY, EVENTDEFAULT_VELOCITY, 48000, woice, this);
    } catch (const QString &e) {
      qDebug() << "Could not preview woice at path" << path << ". Error" << e;
    }
  });
  connect(m_side_menu, &SideMenu::selectWoice, [this](int idx) {
    // TODO: Adjust the length based off pitch and if the instrument loops or
    // not. Also this is variable on tempo rn - fix that.
    if (idx >= 0)
      m_note_preview = std::make_unique<NotePreview>(
          &m_pxtn, EVENTDEFAULT_KEY, EVENTDEFAULT_VELOCITY, 48000,
          m_pxtn.Woice_Get(idx), this);
    else
      m_note_preview = nullptr;
  });
  connect(m_side_menu, &SideMenu::removeUnit, m_keyboard_editor,
          &KeyboardEditor::removeCurrentUnit);

  connect(m_keyboard_editor, &KeyboardEditor::woicesChanged, this,
          &EditorWindow::refreshSideMenuWoices);
  connect(m_side_menu, &SideMenu::playButtonPressed, this,
          &EditorWindow::togglePlayState);
  connect(m_side_menu, &SideMenu::stopButtonPressed, this,
          &EditorWindow::resetAndSuspendAudio);

  connect(m_side_menu, &SideMenu::saveButtonPressed, this, &EditorWindow::save);
  connect(ui->actionSave, &QAction::triggered, this, &EditorWindow::save);
  connect(m_side_menu, &SideMenu::hostButtonPressed, [this]() { Host(true); });
  connect(m_side_menu, &SideMenu::connectButtonPressed, this,
          &EditorWindow::connectToHost);

  connect(ui->actionNewHost, &QAction::triggered, [this]() { Host(false); });
  connect(ui->actionOpenHost, &QAction::triggered, [this]() { Host(true); });
  connect(ui->actionSaveAs, &QAction::triggered, this, &EditorWindow::saveAs);
  connect(ui->actionConnect, &QAction::triggered, this,
          &EditorWindow::connectToHost);
  connect(ui->actionHelp, &QAction::triggered, [=]() {
    QMessageBox::about(
        this, "Help",
        "Ctrl+(Shift)+scroll to zoom.\nShift+scroll to scroll "
        "horizontally.\nMiddle-click drag also "
        "scrolls.\nAlt+Scroll to change quantization.\nShift+click to "
        "seek.\nCtrl+click to modify note instead of on "
        "values.\nCtrl+shift+click to select.\nShift+rclick to deselect.\nWith "
        "a selection, (shift)+(ctrl)+up/down shifts velocity / key.\n (W/S) to "
        "cycle unit.");
  });
  connect(ui->actionExit, &QAction::triggered,
          []() { QApplication::instance()->quit(); });
  connect(ui->actionAbout, &QAction::triggered, [=]() {
    QMessageBox::about(
        this, "About",
        "Experimental collaborative pxtone editor. Special "
        "thanks to all testers and everyone in the pxtone discord!");
  });
}

EditorWindow::~EditorWindow() { delete ui; }

// TODO: Factor this out into a PxtoneAudioPlayer class. Setting play state,
// seeking. Unfortunately start / stop don't even fix this because stopping
// still waits for the buffer to drain (instead of flushing and throwing away)
void EditorWindow::togglePlayState() {
  if (m_audio->state() == QAudio::SuspendedState) {
    m_audio->resume();
    m_side_menu->setPlay(true);
  } else {
    m_audio->suspend();
    m_side_menu->setPlay(false);
  }
}

void EditorWindow::resetAndSuspendAudio() {
  m_keyboard_editor->seekPosition(0);
  m_audio->suspend();

  m_side_menu->setPlay(false);
  // This reset should really also clear the buffers in the audio output, but
  // [reset] unfortunately also disconnects from moo.
}

void EditorWindow::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
    case Qt::Key_Space:
      togglePlayState();
      break;
    case Qt::Key_Escape:
      resetAndSuspendAudio();
      m_keyboard_editor->setFocus(Qt::OtherFocusReason);
      break;
      // TODO: Sort these keys
    case Qt::Key_W:
      m_keyboard_editor->cycleCurrentUnit(-1);
      break;
    case Qt::Key_S:
      if (event->modifiers() & Qt::ControlModifier) {
        if (event->modifiers() & Qt::ShiftModifier)
          saveAs();
        else
          save();
      } else
        m_keyboard_editor->cycleCurrentUnit(1);
      break;
    case Qt::Key_A:
      if (event->modifiers() & Qt::ControlModifier)
        m_keyboard_editor->selectAll();
      else
        m_keyboard_editor->toggleShowAllUnits();
      break;
    case Qt::Key_C:
      if (event->modifiers() & Qt::ControlModifier)
        m_keyboard_editor->copySelection();
      break;
    case Qt::Key_N:
      if (event->modifiers() & Qt::ControlModifier) Host(false);
      break;
    case Qt::Key_O:
      if (event->modifiers() & Qt::ControlModifier) {
        if (event->modifiers() & Qt::ShiftModifier)
          Host(true);
        else
          connectToHost();
      }
      break;
    case Qt::Key_H:
      if (event->modifiers() & Qt::ShiftModifier) {
        m_keyboard_editor->toggleTestActivity();
      }
      break;
    case Qt::Key_V:
      if (event->modifiers() & Qt::ControlModifier) m_keyboard_editor->paste();
      break;
    case Qt::Key_X:
      if (event->modifiers() & Qt::ControlModifier)
        m_keyboard_editor->cutSelection();
      break;
    case Qt::Key_Z:
      if (event->modifiers() & Qt::ControlModifier) {
        if (event->modifiers() & Qt::ShiftModifier)
          m_client->sendAction(UndoRedo::REDO);
        else
          m_client->sendAction(UndoRedo::UNDO);
      }
      break;
    case Qt::Key_Y:
      if (event->modifiers() & Qt::ControlModifier)
        m_client->sendAction(UndoRedo::REDO);
      break;
    case Qt::Key_Backspace:
      if (event->modifiers() & Qt::ControlModifier &&
          event->modifiers() & Qt::ShiftModifier)
        m_keyboard_editor->removeCurrentUnit();
      break;
    case Qt::Key_Delete:
      m_keyboard_editor->clearSelection();
      break;
    case Qt::Key_Up:
    case Qt::Key_Down:
      Direction dir =
          (event->key() == Qt::Key_Up ? Direction::UP : Direction::DOWN);
      bool wide = (event->modifiers() & Qt::ControlModifier);
      bool shift = (event->modifiers() & Qt::ShiftModifier);
      m_keyboard_editor->transposeSelection(dir, wide, shift);
      break;
  }
}

void EditorWindow::refreshSideMenuWoices() {
  QStringList woices;
  for (int i = 0; i < m_pxtn.Woice_Num(); ++i)
    woices.append(m_pxtn.Woice_Get(i)->get_name_buf(nullptr));
  m_side_menu->setWoiceList(QStringList(woices));
}

bool EditorWindow::loadDescriptor(pxtnDescriptor &desc) {
  // An empty desc is interpreted as an empty file so we don't error.
  m_units->beginRefresh();
  if (desc.get_size_bytes() > 0) {
    if (m_pxtn.read(&desc) != pxtnOK) {
      qWarning() << "Error reading pxtone data from descriptor";
      return false;
    }
  }
  // TODO: this unit ID map should be much closer to the service.
  m_keyboard_editor->resetUnitIdMap();
  if (m_pxtn.tones_ready() != pxtnOK) {
    qWarning() << "Error getting tones ready";
    return false;
  }
  m_units->endRefresh();
  resetAndSuspendAudio();

  m_pxtn_device.open(QIODevice::ReadOnly);
  m_audio->start(&m_pxtn_device);
  m_audio->suspend();

  refreshSideMenuWoices();

  m_keyboard_editor->updateGeometry();
  return true;
}
const QString PTCOP_DIR_KEY("ptcop_dir");
void EditorWindow::Host(bool load_file) {
  if (m_server) {
    auto result = QMessageBox::question(this, "Server already running",
                                        "Stop the server and start a new one?");
    if (result != QMessageBox::Yes) return;
  }
  QString filename = "";
  if (load_file) {
    QSettings settings;
    filename = QFileDialog::getOpenFileName(
        this, "Open file", settings.value(PTCOP_DIR_KEY).toString(),
        "pxtone projects (*.ptcop)");

    if (!filename.isEmpty())
      settings.setValue(PTCOP_DIR_KEY, QFileInfo(filename).absolutePath());

    // filename =
    //    "/home/steven/Projects/Music/pxtone/Examples/for-web/basic-rect.ptcop";
  }

  if (load_file && filename.isEmpty()) return;
  bool ok;
  int port =
      QInputDialog::getInt(this, "Port", "What port should this server run on?",
                           15835, 0, 65536, 1, &ok);
  if (!ok) return;
  QString username =
      QInputDialog::getText(this, "Username", "What's your display name?",
                            QLineEdit::Normal, "Anonymous", &ok);
  if (!ok) return;
  if (m_server) {
    delete m_server;
    m_server = nullptr;
    m_server_status->setText("Not hosting");
    qDebug() << "Stopped old server";
  }
  try {
    if (load_file) {
      QFile file(filename);
      if (!file.open(QIODevice::ReadOnly))
        // TODO: Probably make a class for common kinds of errors like this.
        throw QString("Could not read file.");

      m_server =
          new BroadcastServer(file.readAll(), port, this);  //, 3000, 0.3);
    } else
      m_server =
          new BroadcastServer(QByteArray(), port, this);  // , 3000, 0.3);
  } catch (QString e) {
    QMessageBox::critical(this, "Server startup error", e);
    return;
  }
  m_server_status->setText(tr("Hosting on port %1").arg(m_server->port()));
  m_filename = filename;

  m_client->connectToServer("localhost", port, username);
}

bool EditorWindow::saveToFile(QString filename) {
#ifdef _WIN32
  FILE *f_raw;
  _wfopen_s(&f_raw, filename.toStdWString().c_str(), L"wb");
#else
  FILE *f_raw = fopen(filename.toStdString().c_str(), "wb");
#endif
  std::unique_ptr<std::FILE, decltype(&fclose)> f(f_raw, &fclose);
  if (!f) {
    qWarning() << "Could not open file" << filename;
    return false;
  }
  pxtnDescriptor desc;
  if (!desc.set_file_w(f.get())) return false;
  int version_from_pxtn_service = 5;
  if (m_pxtn.write(&desc, false, version_from_pxtn_service) != pxtnOK)
    return false;
  m_side_menu->setModified(false);
  return true;
}
void EditorWindow::saveAs() {
  QSettings settings;
  QString filename = QFileDialog::getSaveFileName(
      this, "Open file", settings.value(PTCOP_DIR_KEY).toString(),
      "pxtone projects (*.ptcop)");
  if (filename.isEmpty()) return;

  settings.setValue(PTCOP_DIR_KEY, QFileInfo(filename).absolutePath());

  if (QFileInfo(filename).suffix() != "ptcop") filename += ".ptcop";
  if (saveToFile(filename)) m_filename = filename;
}
void EditorWindow::save() {
  if (m_filename == "")
    saveAs();
  else
    saveToFile(m_filename);
}
void EditorWindow::connectToHost() {
  if (m_server) {
    auto result =
        QMessageBox::question(this, "Server running", "Stop the server first?");
    if (result != QMessageBox::Yes)
      return;
    else {
      delete m_server;
      m_server = nullptr;
    }
  }
  bool ok;
  QString host =
      QInputDialog::getText(this, "Host", "What host should I connect to?",
                            QLineEdit::Normal, "localhost", &ok);
  if (!ok) return;

  int port = QInputDialog::getInt(
      this, "Port", "What port should I connect to?", 15835, 0, 65536, 1, &ok);
  if (!ok) return;

  QString username =
      QInputDialog::getText(this, "Username", "What's your display name?",
                            QLineEdit::Normal, "Anonymous", &ok);
  if (!ok) return;

  // TODO: some validation here? e.g., maybe disallow exotic chars in case
  // type isn't supported on other clients?
  m_client->connectToServer(host, port, username);
}
