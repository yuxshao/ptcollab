#include "EditorWindow.h"

#include <QDebug>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QVBoxLayout>
#include <QtMultimedia/QAudioDeviceInfo>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>

#include "ComboOptions.h"
#include "FileSettings.h"
#include "pxtone/pxtnDescriptor.h"
#include "ui_EditorWindow.h"
#include "views/MeasureView.h"
#include "views/MooClock.h"
#include "views/ParamView.h"

// TODO: Maybe we could not hard-code this and change the engine to be dynamic
// w/ smart pointers.
static constexpr int EVENT_MAX = 1000000;

EditorWindow::EditorWindow(QWidget *parent)
    : QMainWindow(parent),
      m_server(nullptr),
      m_filename(""),
      m_connection_status(new ConnectionStatusLabel(this)),
      m_fps_status(new QLabel("FPS", this)),
      m_ping_status(new QLabel("", this)),
      m_modified(false),
      m_host_dialog(new HostDialog),
      m_connect_dialog(new ConnectDialog),
      ui(new Ui::EditorWindow) {
  m_pxtn.init_collage(EVENT_MAX);
  int channel_num = 2;
  int sample_rate = 44100;
  m_pxtn.set_destination_quality(channel_num, sample_rate);
  ui->setupUi(this);
  resize(QDesktopWidget().availableGeometry(this).size() * 0.7);

  m_splitter = new QSplitter(Qt::Horizontal, this);
  setCentralWidget(m_splitter);

  m_client = new PxtoneClient(&m_pxtn, m_connection_status, this);
  m_moo_clock = new MooClock(m_client);

  m_keyboard_view = new KeyboardView(m_client, m_moo_clock, nullptr);

  statusBar()->addPermanentWidget(m_fps_status);
  statusBar()->addPermanentWidget(m_ping_status);
  statusBar()->addPermanentWidget(m_connection_status);

  m_side_menu = new PxtoneSideMenu(m_client, this);
  m_measure_splitter = new QFrame(m_splitter);
  QVBoxLayout *measure_layout = new QVBoxLayout(m_measure_splitter);
  m_measure_splitter->setLayout(measure_layout);
  m_measure_splitter->setFrameStyle(QFrame::StyledPanel);
  measure_layout->setContentsMargins(0, 0, 0, 0);
  measure_layout->setSpacing(0);
  m_splitter->addWidget(m_side_menu);
  m_splitter->addWidget(m_measure_splitter);
  m_splitter->setSizes(QList{10, 10000});

  m_key_splitter = new QSplitter(Qt::Vertical, m_splitter);
  m_scroll_area = new EditorScrollArea(m_key_splitter, true);
  m_scroll_area->setWidget(m_keyboard_view);
  // TODO: find a better place for this.
  connect(m_keyboard_view, &KeyboardView::ensureVisibleX,
          [this](int x, bool strict) {
            if (!strict)
              m_scroll_area->ensureWithinMargin(x, -0.02, 0.15, 0.15, 0.75);
            else
              m_scroll_area->ensureWithinMargin(x, 0.5, 0.5, 0.5, 0.5);
          });
  connect(m_scroll_area, &EditorScrollArea::viewportChanged,
          [this](const QRect &viewport) {
            m_client->changeEditState(
                [&](EditState &e) { e.viewport = viewport; }, true);
          });
  connect(m_client, &PxtoneClient::followActivity, [this](const EditState &r) {
    m_client->changeEditState(
        [&](EditState &e) {
          double startClock = r.scale.clockPerPx * r.viewport.left();
          double startPitch = r.scale.pitchPerPx * r.viewport.top();
          QRect old = e.viewport;
          e = r;
          e.scale.clockPerPx =
              e.scale.clockPerPx * e.viewport.width() / old.width();
          e.scale.pitchPerPx =
              e.scale.pitchPerPx * e.viewport.height() / old.height();
          e.viewport = old;

          // disable follow playhead b/c otherwise it could compete with follow
          // user for adjusting scrollbars.
          e.m_follow_playhead = FollowPlayhead::None;
          m_scroll_area->horizontalScrollBar()->setValue(startClock /
                                                         e.scale.clockPerPx);
          m_scroll_area->verticalScrollBar()->setValue(startPitch /
                                                       e.scale.pitchPerPx);
        },
        true);
  });
  connect(m_client->controller(), &PxtoneController::newSong,
          [this]() { m_scroll_area->horizontalScrollBar()->setValue(0); });
  connect(m_client, &PxtoneClient::updatePing,
          [this](std::optional<qint64> ping_ms) {
            if (ping_ms.has_value()) {
              QString s;
              if (ping_ms > 1000)
                s = QString("%1s").arg(ping_ms.value() / 1000);
              else
                s = QString("%1ms").arg(ping_ms.value());
              m_ping_status->setText(QString("Ping: %1").arg(s));
            } else
              m_ping_status->setText("");
          });
  m_scroll_area->setBackgroundRole(QPalette::Dark);
  m_scroll_area->setVisible(true);
  m_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  connect(m_keyboard_view, &KeyboardView::fpsUpdated, [this](qreal fps) {
    m_fps_status->setText(QString("%1 FPS").arg(fps, 0, 'f', 0));
  });

  m_param_scroll_area = new EditorScrollArea(m_key_splitter, false);
  m_param_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_param_scroll_area->setWidget(
      new ParamView(m_client, m_moo_clock, m_param_scroll_area));

  m_measure_scroll_area = new EditorScrollArea(m_splitter, false);
  m_measure_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_measure_scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_measure_scroll_area->setWidget(
      new MeasureView(m_client, m_moo_clock, m_measure_scroll_area));
  m_measure_scroll_area->setMaximumHeight(
      m_measure_scroll_area->widget()->sizeHint().height());

  measure_layout->addWidget(m_measure_scroll_area);
  measure_layout->addWidget(m_key_splitter);
  m_key_splitter->addWidget(m_scroll_area);
  m_key_splitter->addWidget(m_param_scroll_area);
  m_param_scroll_area->controlScroll(m_scroll_area, Qt::Horizontal);
  m_scroll_area->controlScroll(m_measure_scroll_area, Qt::Horizontal);
  m_measure_scroll_area->controlScroll(m_param_scroll_area, Qt::Horizontal);
  m_key_splitter->setSizes(QList{10000, 10});
  // m_measure_splitter->setSizes(QList{1, 10000});

  connect(m_side_menu, &SideMenu::saveButtonPressed, this, &EditorWindow::save);
  connect(ui->actionSave, &QAction::triggered, this, &EditorWindow::save);

  connect(m_client->controller(), &PxtoneController::edited, [this]() {
    m_modified = true;
    m_side_menu->setModified(true);
  });

  connect(ui->actionNewHost, &QAction::triggered, [this]() { Host(false); });
  connect(ui->actionOpenHost, &QAction::triggered, [this]() { Host(true); });
  connect(ui->actionSaveAs, &QAction::triggered, this, &EditorWindow::saveAs);
  connect(ui->actionConnect, &QAction::triggered, this,
          &EditorWindow::connectToHost);
  connect(ui->actionClearSettings, &QAction::triggered, [this]() {
    if (QMessageBox::question(this, "Clear settings",
                              "Are you sure you want to clear your settings?"))
      QSettings().clear();
  });
  connect(ui->actionShortcuts, &QAction::triggered, [=]() {
    QMessageBox::about(
        this, "Shortcuts",
        "Ctrl+(Shift)+scroll to zoom.\nShift+scroll to scroll "
        "horizontally.\nMiddle-click drag also "
        "scrolls.\nAlt+Scroll to change quantization.\nShift+click to "
        "seek.\nCtrl+click to modify note instead of on "
        "values.\nCtrl+shift+click to select.\nShift+rclick or Ctrl+D to "
        "deselect.\nWith "
        "a selection, (shift)+(ctrl)+up/down shifts velocity / key.\n (W or "
        "PgUp/S or PgDn) to "
        "cycle unit.\n1..9 to jump to a unit directly.\n(E/D) to cycle "
        "parameter.\nF to follow playhead.\n C to "
        "toggle if a parameter is copied. Velocity is tied to note and on "
        "events.\n(F1..F4) to switch tabs.\nShift+D to toggle a dark piano "
        "roll.");
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

void EditorWindow::keyPressEvent(QKeyEvent *event) {
  int key = event->key();
  switch (key) {
    case Qt::Key_A:
      if (event->modifiers() & Qt::ControlModifier)
        m_keyboard_view->selectAll();
      break;
    case Qt::Key_C:
      if (event->modifiers() & Qt::ControlModifier)
        m_keyboard_view->copySelection();
      else {
        EVENTKIND kind =
            paramOptions[m_client->editState().current_param_kind_idx()].second;
        m_client->clipboard()->setKindIsCopied(
            kind, !m_client->clipboard()->kindIsCopied(kind));
      }
      break;
    case Qt::Key_D:
      if (event->modifiers() & Qt::ControlModifier)
        m_client->deselect();
      else if (event->modifiers() & Qt::ShiftModifier)
        m_keyboard_view->toggleDark();
      else {
        m_client->changeEditState([&](EditState &s) {
          ++s.m_current_param_kind_idx;
          s.m_current_param_kind_idx = s.current_param_kind_idx();
        });
      }
      break;
    case Qt::Key_E:
      m_client->changeEditState([&](EditState &s) {
        --s.m_current_param_kind_idx;
        s.m_current_param_kind_idx = s.current_param_kind_idx();
      });
      break;
    case Qt::Key_F:
      m_client->changeEditState([&](EditState &s) {
        s.m_follow_playhead = s.m_follow_playhead == FollowPlayhead::None
                                  ? FollowPlayhead::Jump
                                  : FollowPlayhead::None;
      });
      break;
    case Qt::Key_H:
      if (event->modifiers() & Qt::ShiftModifier) {
        m_keyboard_view->toggleTestActivity();
      }
      break;
    case Qt::Key_S:
      if (!(event->modifiers() & Qt::ControlModifier))
        m_keyboard_view->cycleCurrentUnit(1);
      break;
    /*case Qt::Key_I:
      m_host_dialog->exec();
      break;*/
    case Qt::Key_W:
      m_keyboard_view->cycleCurrentUnit(-1);
      break;
    case Qt::Key_V:
      if (event->modifiers() & Qt::ControlModifier) m_keyboard_view->paste();
      break;
    case Qt::Key_X:
      if (event->modifiers() & Qt::ControlModifier)
        m_keyboard_view->cutSelection();
      break;
    case Qt::Key_Y:
      if (event->modifiers() & Qt::ControlModifier)
        m_client->sendAction(UndoRedo::REDO);
      break;
    case Qt::Key_Z:
      if (event->modifiers() & Qt::ControlModifier) {
        if (event->modifiers() & Qt::ShiftModifier)
          m_client->sendAction(UndoRedo::REDO);
        else
          m_client->sendAction(UndoRedo::UNDO);
      }
      break;

    case Qt::Key_F1:
    case Qt::Key_F2:
    case Qt::Key_F3:
    case Qt::Key_F4:
      m_side_menu->setTab(key - Qt::Key_F1);
      break;
    case Qt::Key_1:
    case Qt::Key_2:
    case Qt::Key_3:
    case Qt::Key_4:
    case Qt::Key_5:
    case Qt::Key_6:
    case Qt::Key_7:
    case Qt::Key_8:
    case Qt::Key_9:
      m_keyboard_view->setCurrentUnitNo(key - Qt::Key_1);
      break;
    case Qt::Key_0:
      m_keyboard_view->setCurrentUnitNo(9);
      break;
    case Qt::Key_PageDown:
      m_keyboard_view->cycleCurrentUnit(1);
      break;
    case Qt::Key_PageUp:
      m_keyboard_view->cycleCurrentUnit(-1);
      break;
    case Qt::Key_Space:
      m_client->togglePlayState();
      break;
    case Qt::Key_Escape:
      m_client->resetAndSuspendAudio();
      m_keyboard_view->setFocus(Qt::OtherFocusReason);
      break;
    case Qt::Key_Backspace:
      if (event->modifiers() & Qt::ControlModifier &&
          event->modifiers() & Qt::ShiftModifier)
        m_client->removeCurrentUnit();
      break;
    case Qt::Key_Delete:
      m_keyboard_view->clearSelection();
      break;
    case Qt::Key_Up:
    case Qt::Key_Down:
      Direction dir =
          (event->key() == Qt::Key_Up ? Direction::UP : Direction::DOWN);
      bool wide = (event->modifiers() & Qt::ControlModifier);
      bool shift = (event->modifiers() & Qt::ShiftModifier);
      m_keyboard_view->transposeSelection(dir, wide, shift);
      break;
  }
}

bool EditorWindow::maybeSave() {
  if (!m_modified) return true;

  int ret = QMessageBox::question(
      this, tr("Unsaved changes"),
      tr("Do you want to save your changes first?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  switch (ret) {
    case QMessageBox::Save:
      save();
      return true;
    case QMessageBox::Discard:
      return true;
    case QMessageBox::Cancel:
      return false;
      break;
    default:
      qWarning() << "Unexpected maybeSave answer" << ret;
      return false;
  }
}

void EditorWindow::closeEvent(QCloseEvent *event) {
  if (maybeSave())
    event->accept();
  else
    event->ignore();
}

static const QString HISTORY_SAVE_FILE = "history_save_file";
void EditorWindow::Host(bool load_file) {
  if (!maybeSave()) return;
  if (m_server) {
    auto result = QMessageBox::question(this, "Server already running",
                                        "Stop the server and start a new one?");
    if (result != QMessageBox::Yes) return;
  }
  if (!m_host_dialog->start(load_file)) return;
  m_host_dialog->persistSettings();

  std::optional<QString> filename = m_host_dialog->projectName();
  QString username = m_host_dialog->username();
  std::optional<int> port = std::nullopt;
  if (m_host_dialog->port().has_value()) {
    bool ok;
    int p = m_host_dialog->port().value().toInt(&ok);
    if (!ok) {
      QMessageBox::warning(this, tr("Invalid port"), tr("Invalid port"));
      return;
    }
    port = p;
  }

  if (m_server) {
    m_client->disconnectFromServerSuppressSignal();
    delete m_server;
    m_server = nullptr;
    m_connection_status->setServerConnectionState(std::nullopt);
    qDebug() << "Stopped old server";
  }

  std::optional<QString> recording_save_file = m_host_dialog->recordingName();

  try {
    m_server = new BroadcastServer(filename, port, recording_save_file,
                                   this);  // , 3000, 0.3);
  } catch (QString e) {
    QMessageBox::critical(this, "Server startup error", e);
    return;
  }
  m_connection_status->setServerConnectionState(
      QString("%1:%2")
          .arg(m_server->address().toString())
          .arg(m_server->port()));
  m_filename = filename;
  m_modified = false;
  m_side_menu->setModified(false);

  m_client->connectToServer("localhost", m_server->port(), username);
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
  m_modified = false;
  m_side_menu->setModified(false);
  return true;
}
void EditorWindow::saveAs() {
  QSettings settings;
  QString filename = QFileDialog::getSaveFileName(
      this, "Open file",
      QFileInfo(settings.value(PTCOP_FILE_KEY).toString()).absolutePath(),
      "pxtone projects (*.ptcop)");
  if (filename.isEmpty()) return;

  settings.setValue(PTCOP_FILE_KEY, QFileInfo(filename).absoluteFilePath());

  if (QFileInfo(filename).suffix() != "ptcop") filename += ".ptcop";
  if (saveToFile(filename)) m_filename = filename;
}
void EditorWindow::save() {
  if (!m_filename.has_value())
    saveAs();
  else
    saveToFile(m_filename.value());
}

void EditorWindow::connectToHost() {
  if (!maybeSave()) return;
  if (m_server &&
      QMessageBox::question(this, "Server running", "Stop the server first?") !=
          QMessageBox::Yes)
    return;

  if (!m_connect_dialog->exec()) return;
  m_connect_dialog->persistSettings();
  QSettings settings;
  QStringList parsed_host_and_port = m_connect_dialog->address().split(":");
  if (parsed_host_and_port.length() != 2) {
    QMessageBox::warning(this, tr("Invalid address"),
                         tr("Address must be of the form HOST:PORT."));
    return;
  }
  QString host = parsed_host_and_port[0];

  bool ok;
  int port = parsed_host_and_port[1].toInt(&ok);
  if (!ok) {
    QMessageBox::warning(this, tr("Invalid address"),
                         tr("Port must be a number."));
    return;
  }

  // TODO: some validation here? e.g., maybe disallow exotic chars in case
  // type isn't supported on other clients?
  m_modified = false;
  m_side_menu->setModified(false);

  m_client->disconnectFromServerSuppressSignal();
  if (m_server) {
    delete m_server;
    m_server = nullptr;
  }
  m_client->connectToServer(host, port, m_connect_dialog->username());
}
