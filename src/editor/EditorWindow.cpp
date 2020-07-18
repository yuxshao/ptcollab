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

#include "ParamView.h"
#include "pxtone/pxtnDescriptor.h"
#include "ui_EditorWindow.h"

// TODO: Maybe we could not hard-code this and change the engine to be dynamic
// w/ smart pointers.
static constexpr int EVENT_MAX = 1000000;

EditorWindow::EditorWindow(QWidget *parent)
    : QMainWindow(parent),
      m_server(nullptr),
      m_filename(""),
      m_server_status(new QLabel("Not hosting", this)),
      m_client_status(new QLabel("Not connected", this)),
      ui(new Ui::EditorWindow) {
  m_pxtn.init_collage(EVENT_MAX);
  int channel_num = 2;
  int sample_rate = 44100;
  m_pxtn.set_destination_quality(channel_num, sample_rate);
  ui->setupUi(this);
  resize(QDesktopWidget().availableGeometry(this).size() * 0.7);

  m_splitter = new QSplitter(Qt::Horizontal, this);
  setCentralWidget(m_splitter);

  m_units = new UnitListModel(&m_pxtn, this);
  m_client = new PxtoneClient(&m_pxtn, m_client_status, m_units, this);
  m_keyboard_view = new KeyboardView(m_client, nullptr);

  statusBar()->addPermanentWidget(m_server_status);
  statusBar()->addPermanentWidget(m_client_status);

  m_side_menu = new PxtoneSideMenu(m_client, m_units, this);
  m_key_splitter = new QSplitter(Qt::Vertical, m_splitter);
  m_splitter->addWidget(m_side_menu);
  m_splitter->addWidget(m_key_splitter);
  m_splitter->setSizes(QList{10, 10000});

  m_scroll_area = new EditorScrollArea(m_key_splitter);
  m_scroll_area->setWidget(m_keyboard_view);
  m_scroll_area->setBackgroundRole(QPalette::Dark);
  m_scroll_area->setVisible(true);
  m_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  // TODO: Make member var
  EditorScrollArea *m_param_scroll_area = new EditorScrollArea(m_key_splitter);
  m_param_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_param_scroll_area->setWidget(new ParamView(m_client, m_param_scroll_area));
  m_key_splitter->addWidget(m_scroll_area);
  m_key_splitter->addWidget(m_param_scroll_area);
  m_param_scroll_area->controlScroll(m_scroll_area, Qt::Horizontal);
  m_scroll_area->controlScroll(m_param_scroll_area, Qt::Horizontal);
  m_key_splitter->setSizes(QList{10000, 10});

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
        "values.\nCtrl+shift+click to select.\nShift+rclick or Ctrl+D to "
        "deselect.\nWith "
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

void EditorWindow::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
    case Qt::Key_Space:
      m_client->togglePlayState();
      break;
    case Qt::Key_Escape:
      m_client->resetAndSuspendAudio();
      m_keyboard_view->setFocus(Qt::OtherFocusReason);
      break;
      // TODO: Sort these keys
    case Qt::Key_W:
      m_keyboard_view->cycleCurrentUnit(-1);
      break;
    case Qt::Key_S:
      if (event->modifiers() & Qt::ControlModifier) {
        if (event->modifiers() & Qt::ShiftModifier)
          saveAs();
        else
          save();
      } else
        m_keyboard_view->cycleCurrentUnit(1);
      break;
    case Qt::Key_A:
      if (event->modifiers() & Qt::ControlModifier)
        m_keyboard_view->selectAll();
      break;
    case Qt::Key_C:
      if (event->modifiers() & Qt::ControlModifier)
        m_keyboard_view->copySelection();
      break;
    case Qt::Key_D:
      if (event->modifiers() & Qt::ControlModifier) m_keyboard_view->deselect();
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
        m_keyboard_view->toggleTestActivity();
      }
      break;
    case Qt::Key_V:
      if (event->modifiers() & Qt::ControlModifier) m_keyboard_view->paste();
      break;
    case Qt::Key_X:
      if (event->modifiers() & Qt::ControlModifier)
        m_keyboard_view->cutSelection();
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
