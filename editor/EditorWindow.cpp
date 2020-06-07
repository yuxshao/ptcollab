#include "EditorWindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QtMultimedia/QAudioDeviceInfo>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>

#include "pxtone/pxtnDescriptor.h"
#include "server/BroadcastServer.h"
#include "server/Client.h"
#include "ui_EditorWindow.h"

// TODO: Maybe we could not hard-code this and change the engine to be dynamic
// w/ smart pointers.
static constexpr int EVENT_MAX = 1000000;

EditorWindow::EditorWindow(QWidget *parent)
    : QMainWindow(parent),
      m_pxtn_device(this, &m_pxtn),
      m_server(nullptr),
      m_client(new Client(this)),
      m_filename(""),
      m_server_status(new QLabel("Not hosting", this)),
      m_client_status(new QLabel("Not connected", this)),
      ui(new Ui::EditorWindow) {
  m_pxtn.init_collage(EVENT_MAX);
  int channel_num = 2;
  int sample_rate = 44100;
  m_pxtn.set_destination_quality(channel_num, sample_rate);
  ui->setupUi(this);

  QAudioFormat format;
  format.setSampleRate(sample_rate);
  format.setChannelCount(channel_num);
  format.setSampleSize(16);  // fixed in pxtone
  format.setCodec("audio/pcm");
  format.setByteOrder(QAudioFormat::LittleEndian);
  format.setSampleType(QAudioFormat::SignedInt);

  QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
  if (!info.isFormatSupported(format)) {
    qWarning()
        << "Raw audio format not supported by backend, cannot play audio.";
    return;
  }

  m_audio = new QAudioOutput(format, this);

  // m_audio->setBufferSize(441000);

  // Apparently this reduces latency in pulseaudio, but also makes
  // some sounds choppier
  m_audio->setCategory("game");
  m_audio->setVolume(1.0);

  m_splitter = new QSplitter(Qt::Horizontal, this);
  setCentralWidget(m_splitter);

  m_keyboard_editor = new KeyboardEditor(&m_pxtn, m_audio, m_client);
  connect(m_client, &Client::connected,
          [this](pxtnDescriptor &desc,
                 const QList<RemoteActionWithUid> &history, qint64 uid) {
            HostAndPort host_and_port = m_client->currentlyConnectedTo();
            m_client_status->setText(tr("Connected to %1:%2")
                                         .arg(host_and_port.host)
                                         .arg(host_and_port.port));
            QMessageBox::information(this, "Connected", "Connected to server.");
            loadDescriptor(desc);
            m_keyboard_editor->setUid(uid);
            m_keyboard_editor->loadHistory(history);
          });
  connect(m_client, &Client::disconnected, [this]() {
    m_client_status->setText(tr("Not connected"));
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

  m_side_menu = new SideMenu;
  m_splitter->addWidget(m_side_menu);
  m_splitter->addWidget(m_scroll_area);
  m_splitter->setSizes(QList{10, 10000});

  connect(m_side_menu, &SideMenu::quantXUpdated, m_keyboard_editor,
          &KeyboardEditor::setQuantX);
  connect(m_side_menu, &SideMenu::quantYUpdated, m_keyboard_editor,
          &KeyboardEditor::setQuantY);
  connect(m_side_menu, &SideMenu::selectedUnitChanged, m_keyboard_editor,
          &KeyboardEditor::setCurrentUnit);
  connect(m_side_menu, &SideMenu::showAllChanged, m_keyboard_editor,
          &KeyboardEditor::setShowAll);
  connect(m_keyboard_editor, &KeyboardEditor::currentUnitChanged, m_side_menu,
          &SideMenu::setSelectedUnit);
  connect(m_keyboard_editor, &KeyboardEditor::showAllChanged, m_side_menu,
          &SideMenu::setShowAll);
  connect(m_keyboard_editor, &KeyboardEditor::onEdit,
          [=]() { m_side_menu->setModified(true); });

  connect(m_side_menu, &SideMenu::playButtonPressed, this,
          &EditorWindow::togglePlayState);
  connect(m_side_menu, &SideMenu::stopButtonPressed, this,
          &EditorWindow::resetAndSuspendAudio);

  connect(m_side_menu, &SideMenu::saveButtonPressed, this, &EditorWindow::save);
  connect(ui->actionSave, &QAction::triggered, this, &EditorWindow::save);
  connect(m_side_menu, &SideMenu::hostButtonPressed, this,
          &EditorWindow::loadFileAndHost);
  connect(m_side_menu, &SideMenu::connectButtonPressed, this,
          &EditorWindow::connectToHost);
  connect(ui->actionHost, &QAction::triggered, this,
          &EditorWindow::loadFileAndHost);
  connect(ui->actionSaveAs, &QAction::triggered, this, &EditorWindow::saveAs);
  connect(ui->actionConnect, &QAction::triggered, this,
          &EditorWindow::connectToHost);
  connect(ui->actionAbout, &QAction::triggered, [=]() {
    QMessageBox::about(
        this, "About",
        "Experimental editor for pxtone files in Qt. Some goals: "
        "faster workflow with more shortcuts, (not yet) collaborative "
        "editing.");
  });
}

EditorWindow::~EditorWindow() { delete ui; }

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
  pxtnVOMITPREPARATION prep{};
  prep.flags |= pxtnVOMITPREPFLAG_loop;
  prep.start_pos_float = 0;
  prep.master_volume = 0.80f;
  bool success = m_pxtn.moo_preparation(&prep);
  if (!success) qWarning() << "Moo preparation error";
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
      break;
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
      m_keyboard_editor->toggleShowAllUnits();
      break;
    case Qt::Key_O:
      if (event->modifiers() & Qt::ControlModifier) {
        if (event->modifiers() & Qt::ShiftModifier)
          loadFileAndHost();
        else
          connectToHost();
      }
      break;
    case Qt::Key_Z:
      if (event->modifiers() & Qt::ControlModifier) {
        if (event->modifiers() & Qt::ShiftModifier)
          m_keyboard_editor->redo();
        else
          m_keyboard_editor->undo();
      }
      break;
    case Qt::Key_Y:
      if (event->modifiers() & Qt::ControlModifier) m_keyboard_editor->redo();
      break;
  }
}

bool EditorWindow::loadDescriptor(pxtnDescriptor &desc) {
  if (m_pxtn.read(&desc) != pxtnOK) {
    qWarning() << "Error reading descriptor";
    return false;
  }
  if (m_pxtn.tones_ready() != pxtnOK) {
    qWarning() << "Error getting tones ready";
    return false;
  }
  resetAndSuspendAudio();

  m_pxtn_device.open(QIODevice::ReadOnly);
  m_audio->start(&m_pxtn_device);
  m_audio->suspend();
  std::vector<QString> units;
  for (int i = 0; i < m_pxtn.Unit_Num(); ++i)
    units.push_back(QString(m_pxtn.Unit_Get(i)->get_name_buf(nullptr)));
  m_side_menu->setUnits(units);
  m_keyboard_editor->updateGeometry();
  return true;
}

void EditorWindow::loadFile(QString filename) {
  std::unique_ptr<std::FILE, decltype(&fclose)> f(
      fopen(filename.toStdString().c_str(), "rb"), &fclose);
  if (!f) {
    qWarning() << "Could not open file";
    return;
  }
  pxtnDescriptor desc;
  desc.set_file_r(f.get());
  if (m_pxtn.read(&desc) != pxtnOK) {
    qWarning() << "Error reading file" << filename;
    return;
  }
  if (m_pxtn.tones_ready() != pxtnOK) {
    qWarning() << "Error getting tones ready";
    return;
  }
  resetAndSuspendAudio();

  loadDescriptor(desc);
}

void EditorWindow::loadFileAndHost() {
  if (m_server) {
    auto result = QMessageBox::question(this, "Server already running",
                                        "Stop the server and start a new one?");
    if (result != QMessageBox::Yes) return;
  }
  QString filename = QFileDialog::getOpenFileName(this, "Open file", "",
                                                  "pxtone projects (*.ptcop)");
  if (filename.length() == 0) {
    qDebug() << "Aborting, no file selected";
    return;
  }
  bool ok;
  int port =
      QInputDialog::getInt(this, "Port", "What port should this server run on?",
                           15835, 0, 65536, 1, &ok);
  if (!ok) {
    qDebug() << "Aborted hosting";
    return;
  }
  if (m_server) {
    delete m_server;
    m_server = nullptr;
    qDebug() << "Stopped old server";
  }
  m_server = new BroadcastServer(filename, port, this);
  m_server_status->setText(tr("Hosting on port %1").arg(m_server->port()));
  connect(m_server, &QObject::destroyed,
          [this]() { m_server_status->setText("Not hosting"); });
  m_filename = filename;

  m_client->connectToServer("localhost", port);
}

bool EditorWindow::saveToFile(QString filename) {
  std::unique_ptr<std::FILE, decltype(&fclose)> f(
      fopen(filename.toStdString().c_str(), "w"), &fclose);
  if (!f) {
    qWarning() << "Could not open file";
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
  QString filename = QFileDialog::getSaveFileName(this, "Open file", "",
                                                  "pxtone projects (*.ptcop)");
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
  if (!ok) {
    qDebug() << "Aborted connecting";
    return;
  }
  int port = QInputDialog::getInt(
      this, "Port", "What port should I connect to?", 15835, 0, 65536, 1, &ok);
  if (!ok) {
    qDebug() << "Aborted connecting";
    return;
  }
  m_client->connectToServer(host, port);
}
