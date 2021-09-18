#include "EditorWindow.h"

#include <QDebug>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QSaveFile>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QVBoxLayout>
#include <QtMultimedia/QAudioDeviceInfo>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>

#include "ComboOptions.h"
#include "InputEvent.h"
#include "Settings.h"
#include "WelcomeDialog.h"
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
      m_filename(std::nullopt),
      m_connection_status(new ConnectionStatusLabel(this)),
      m_fps_status(new QLabel("FPS", this)),
      m_ping_status(new QLabel("", this)),
      m_modified(false),
      m_host_dialog(new HostDialog(this)),
      m_welcome_dialog(new WelcomeDialog(this)),
      m_connect_dialog(new ConnectDialog(this)),
      m_shortcuts_dialog(new ShortcutsDialog(this)),
      m_render_dialog(new RenderDialog(this)),
      m_midi_wrapper(new MidiWrapper()),
      m_settings_dialog(new SettingsDialog(m_midi_wrapper, this)),
      m_copy_options_dialog(nullptr),
      ui(new Ui::EditorWindow) {
  m_pxtn.init_collage(EVENT_MAX);
  int channel_num = 2;
  int sample_rate = 44100;
  m_pxtn.set_destination_quality(channel_num, sample_rate);
  ui->setupUi(this);
  resize(QDesktopWidget().availableGeometry(this).size() * 0.7);
  setAcceptDrops(true);

  m_splitter = new QSplitter(Qt::Horizontal, this);
  setCentralWidget(m_splitter);

  m_client = new PxtoneClient(&m_pxtn, m_connection_status, this);
  m_moo_clock = new MooClock(m_client);

  m_copy_options_dialog = new CopyOptionsDialog(m_client->clipboard(), this);

  m_keyboard_view = new KeyboardView(m_client, m_moo_clock, nullptr);

  statusBar()->addPermanentWidget(m_fps_status);
  statusBar()->addPermanentWidget(m_ping_status);
  statusBar()->addPermanentWidget(m_connection_status);

  m_side_menu = new PxtoneSideMenu(m_client, m_moo_clock, this);
  m_measure_splitter = new QFrame(m_splitter);
  QVBoxLayout *measure_layout = new QVBoxLayout(m_measure_splitter);
  m_measure_splitter->setLayout(measure_layout);
  m_measure_splitter->setFrameStyle(QFrame::StyledPanel);
  measure_layout->setContentsMargins(0, 0, 0, 0);
  measure_layout->setSpacing(0);
  m_splitter->addWidget(m_side_menu);
  m_splitter->addWidget(m_measure_splitter);
  m_splitter->setSizes(Settings::SideMenuWidth::get());
  connect(m_splitter, &QSplitter::splitterMoved, [this](int, int) {
    Settings::SideMenuWidth::set(m_splitter->sizes());
  });

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
  m_key_splitter->setSizes(Settings::BottomBarHeight::get());
  connect(m_key_splitter, &QSplitter::splitterMoved, [this](int, int) {
    Settings::BottomBarHeight::set(m_key_splitter->sizes());
  });
  // m_measure_splitter->setSizes(QList{1, 10000});

  connect(m_side_menu, &SideMenu::saveButtonPressed, this, &EditorWindow::save);
  connect(ui->actionSave, &QAction::triggered, this, &EditorWindow::save);

  connect(m_client->controller(), &PxtoneController::edited, [this]() {
    m_modified = true;
    m_side_menu->setModified(true);
  });

  connect(ui->actionNewHost, &QAction::triggered,
          [this]() { Host(HostSetting::NewFile); });
  connect(ui->actionOpenHost, &QAction::triggered,
          [this]() { Host(HostSetting::LoadFile); });
  connect(ui->actionSaveAs, &QAction::triggered, this, &EditorWindow::saveAs);
  connect(ui->actionRender, &QAction::triggered, this, &EditorWindow::render);
  connect(ui->actionConnect, &QAction::triggered, this,
          &EditorWindow::connectToHost);
  connect(ui->actionClear_Settings, &QAction::triggered, [this]() {
    if (QMessageBox::question(this, tr("Clear settings"),
                              tr("Are you sure you want to clear your app "
                                 "settings?")))
      QSettings().clear();
  });
  connect(ui->actionDecrease_font_size, &QAction::triggered,
          &Settings::TextSize::decrease);
  connect(ui->actionIncrease_font_size, &QAction::triggered,
          &Settings::TextSize::increase);
  connect(ui->actionShortcuts, &QAction::triggered, m_shortcuts_dialog,
          &QDialog::exec);
  connect(ui->actionExit, &QAction::triggered,
          []() { QApplication::instance()->quit(); });
  connect(ui->actionAbout, &QAction::triggered, [=]() {
    QMessageBox::about(this, "About",
                       tr("Multiplayer pxtone music editor. Special "
                          "thanks to all testers and everyone in the pxtone "
                          "discord!\n\nVersion: "
                          "%1")
                           .arg(QApplication::applicationVersion()));
  });
  connect(ui->actionOptions, &QAction::triggered, this->m_settings_dialog,
          &QDialog::show);
  connect(m_settings_dialog, &SettingsDialog::midiPortSelected,
          [this](int port_no) {
            qDebug() << "using midi port" << port_no;
            m_midi_wrapper->usePort(port_no, [this](Input::Event::Event x) {
              qDebug() << "Received event";
              QCoreApplication::postEvent(this, new InputEvent(x),
                                          Qt::HighEventPriority);
            });
          });
  connect(ui->actionClean, &QAction::triggered, [&]() {
    auto result =
        QMessageBox::question(this, tr("Clean units / voices"),
                              tr("Are you sure you want to remove all unused "
                                 "voices and units? This cannot be undone."));
    if (result != QMessageBox::Yes) return;
    m_client->removeUnusedUnitsAndWoices();
  });
  connect(ui->actionCopyOptions, &QAction::triggered, [this]() {
    m_copy_options_dialog->setVisible(!m_copy_options_dialog->isVisible());
  });
  connect(m_welcome_dialog, &WelcomeDialog::newSelected,
          [this]() { Host(HostSetting::NewFile); });
  connect(m_welcome_dialog, &WelcomeDialog::openSelected,
          [this]() { Host(HostSetting::LoadFile); });
  connect(m_welcome_dialog, &WelcomeDialog::connectSelected, this,
          &EditorWindow::connectToHost);

  if (Settings::ShowWelcomeDialog::get()) {
    // In a timer so that the main window has time to show up
    QTimer::singleShot(0, m_welcome_dialog, &QDialog::exec);
  }
}

EditorWindow::~EditorWindow() { delete ui; }

// #define DEBUG_RECORD_INPUT

void EditorWindow::keyReleaseEvent(QKeyEvent *event) {
  if (!event->isAutoRepeat()) {
    int key = event->key();
    switch (key) {
      case Qt::Key::Key_B:
#ifdef DEBUG_RECORD_INPUT
        recordInput(Input::Event::Off{EVENTDEFAULT_KEY});
#endif
        break;
      case Qt::Key::Key_N:
#ifdef DEBUG_RECORD_INPUT
        recordInput(Input::Event::Off{EVENTDEFAULT_KEY + 256});
#endif
        break;
    }
  }
}

void EditorWindow::keyPressEvent(QKeyEvent *event) {
  int key = event->key();
  switch (key) {
    case Qt::Key_A:
      if (event->modifiers() & Qt::ControlModifier) {
        if (event->modifiers() & Qt::ShiftModifier) {
          m_client->selectAllUnits(true);
        } else
          m_keyboard_view->selectAll(false);
      }
      break;
    case Qt::Key_B:
#ifdef DEBUG_RECORD_INPUT
      if (!event->isAutoRepeat())
        recordInput(Input::Event::On{EVENTDEFAULT_KEY, 127});
#endif
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
      if (event->modifiers() & Qt::ControlModifier) {
        if (event->modifiers() & Qt::ShiftModifier) {
          m_client->selectAllUnits(false);
        } else
          m_client->deselect(false);
      } else if (event->modifiers() & Qt::ShiftModifier)
        m_keyboard_view->toggleDark();
      else {
        m_client->changeEditState(
            [&](EditState &s) {
              ++s.m_current_param_kind_idx;
              s.m_current_param_kind_idx = s.current_param_kind_idx();
            },
            false);
      }
      break;
    case Qt::Key_E:
      m_client->changeEditState(
          [&](EditState &s) {
            --s.m_current_param_kind_idx;
            s.m_current_param_kind_idx = s.current_param_kind_idx();
          },
          false);
      break;
    case Qt::Key_F:
      m_client->changeEditState(
          [&](EditState &s) {
            s.m_follow_playhead = s.m_follow_playhead == FollowPlayhead::None
                                      ? FollowPlayhead::Jump
                                      : FollowPlayhead::None;
          },
          false);
      break;
    /*case Qt::Key_H:
      if (event->modifiers() & Qt::ShiftModifier) {
        m_keyboard_view->toggleTestActivity();
      }
      break;*/
    case Qt::Key_I:
      m_client->changeEditState(
          [&](EditState &e) {
            auto &qx = e.m_quantize_clock_idx;
            if (qx > 0) qx--;
          },
          false);
      break;
    case Qt::Key_J:
      // if (!event->isAutoRepeat())
      recordInput(Input::Event::Skip{-1});
      break;
    case Qt::Key_K:
      m_client->changeEditState(
          [&](EditState &e) {
            auto &qx = e.m_quantize_clock_idx;
            int size = sizeof(quantizeXOptions) / sizeof(quantizeXOptions[0]);
            if (qx < size - 1) qx++;
          },
          false);
      break;
    case Qt::Key_L:
      if (event->modifiers() & Qt::ControlModifier)
        Settings::AutoAdvance::set(!Settings::AutoAdvance::get());
      else
        recordInput(Input::Event::Skip{1});
      break;
    case Qt::Key_M: {
      std::optional<int> maybe_unit_no =
          m_client->unitIdMap().idToNo(m_client->editState().m_current_unit_id);
      if (maybe_unit_no.has_value()) {
        int unit_no = maybe_unit_no.value();
        if (event->modifiers() & Qt::ShiftModifier)
          m_client->toggleSolo(unit_no);
        else {
          const pxtnUnit *u = m_pxtn.Unit_Get(unit_no);
          if (u) m_client->setUnitPlayed(unit_no, !u->get_played());
        }
      }
      break;
    }
    case Qt::Key_N:
#ifdef DEBUG_RECORD_INPUT
      if (!event->isAutoRepeat())
        recordInput(Input::Event::On{EVENTDEFAULT_KEY + 256, 127});
#endif
      break;
    case Qt::Key_P:
      if (event->modifiers() & Qt::ControlModifier)
        Settings::ChordPreview::set(!Settings::ChordPreview::get());
      break;
    case Qt::Key_Q:
      if (event->modifiers() & Qt::AltModifier)
        m_keyboard_view->quantizeSelection();
      break;
    case Qt::Key_S:
      if (!(event->modifiers() & Qt::ControlModifier))
        m_keyboard_view->cycleCurrentUnit(1);
      break;
    case Qt::Key_W:
      m_keyboard_view->cycleCurrentUnit(-1);
      break;
    case Qt::Key_V:
      if (event->modifiers() & Qt::ControlModifier)
        m_keyboard_view->paste(false);
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
      m_keyboard_view->setCurrentUnitNo(key - Qt::Key_1, false);
      break;
    case Qt::Key_0:
      m_keyboard_view->setCurrentUnitNo(9, false);
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
      else {
        int now = m_moo_clock->now();
        int end = quantize(now - 1, m_client->quantizeClock());
        if (end < 0) end = 0;
        Interval clock_int{end, now};
        int unit_id = m_client->editState().m_current_unit_id;
        std::list<Action::Primitive> actions;
        actions.push_back({EVENTKIND_ON, unit_id, clock_int.start,
                           Action::Delete{clock_int.end}});
        actions.push_back({EVENTKIND_VELOCITY, unit_id, clock_int.start,
                           Action::Delete{clock_int.end}});
        actions.push_back({EVENTKIND_KEY, unit_id, clock_int.start,
                           Action::Delete{clock_int.end}});
        m_client->applyAction(actions);
        m_client->seekMoo(end);
      }
      break;
    case Qt::Key_Delete:
      m_keyboard_view->clearSelection();
      break;
    case Qt::Key_Up:
    case Qt::Key_Down: {
      Direction dir =
          (event->key() == Qt::Key_Up ? Direction::UP : Direction::DOWN);
      bool wide = (event->modifiers() & Qt::ControlModifier);
      bool shift = (event->modifiers() & Qt::ShiftModifier);
      m_keyboard_view->transposeSelection(dir, wide, shift);
    } break;
    case Qt::Key_Left:
    case Qt::Key_Right:
      if (event->modifiers() & Qt::ShiftModifier) {
        bool shift_right = event->key() == Qt::Key_Right;
        bool grow = (event->modifiers() & Qt::ControlModifier);
        tweakSelectionRange(shift_right, grow);
      }
      break;
  }
}

void EditorWindow::tweakSelectionRange(bool shift_right, bool grow) {
  if (m_client->editState().mouse_edit_state.selection == std::nullopt) return;
  qint32 q = m_client->quantizeClock();
  m_client->changeEditState(
      [&](EditState &e) {
        Interval &selection = e.mouse_edit_state.selection.value();
        if (shift_right && grow)
          selection.end = quantize(selection.end + q, q);
        else if (shift_right && !grow)
          selection.start = quantize(selection.start + q, q);
        else if (!shift_right && grow)
          selection.start = std::max(0, quantize(selection.start - q, q));
        else if (!shift_right && !grow)
          selection.end =
              std::max(selection.start, quantize(selection.end - q, q));
      },
      false);
}

void applyOn(const Input::State::On &v, int end, PxtoneClient *client) {
  std::vector<Interval> clock_ints = v.clock_ints(end, client->pxtn()->master);
  // TODO: Dedup
  using namespace Action;
  std::list<Primitive> actions;
  for (const auto &clock_int : clock_ints) {
    actions.push_back({EVENTKIND_ON, client->editState().m_current_unit_id,
                       clock_int.start, Delete{clock_int.end}});
    actions.push_back({EVENTKIND_VELOCITY,
                       client->editState().m_current_unit_id, clock_int.start,
                       Delete{clock_int.end}});
    actions.push_back({EVENTKIND_KEY, client->editState().m_current_unit_id,
                       clock_int.start, Delete{clock_int.end}});
    actions.push_back({EVENTKIND_ON, client->editState().m_current_unit_id,
                       clock_int.start, Add{clock_int.length()}});
    actions.push_back({EVENTKIND_VELOCITY,
                       client->editState().m_current_unit_id, clock_int.start,
                       Add{v.on.vel}});
    actions.push_back({EVENTKIND_KEY, client->editState().m_current_unit_id,
                       clock_int.start, Add{v.on.key}});
  }
  if (actions.size() > 0) client->applyAction(actions);
}

void EditorWindow::recordInput(const Input::Event::Event &e) {
  std::visit(
      overloaded{
          [this](const Input::Event::On &e) {
            // TODO: handle repeat
            int start = m_moo_clock->nowNoWrap();
            m_client->changeEditState(
                [&](EditState &state) {
                  if (state.m_input_state.has_value()) {
                    applyOn(state.m_input_state.value(), start, m_client);
                  }
                  state.m_input_state = Input::State::On{start, e};
                },
                false);
            auto maybe_unit_no = m_client->unitIdMap().idToNo(
                m_client->editState().m_current_unit_id);
            if (maybe_unit_no != std::nullopt) {
              qint32 unit_no = maybe_unit_no.value();
              int key = Settings::PolyphonicMidiNotePreview::get() ? e.key : -1;
              bool chordPreview =
                  (!Settings::PolyphonicMidiNotePreview::get()) &&
                  Settings::ChordPreview::get() && !m_client->isPlaying();
              m_record_note_preview[key] = std::make_unique<NotePreview>(
                  &m_pxtn, &m_client->moo()->params, unit_no, start, e.key,
                  e.vel, m_client->audioState()->bufferSize(), chordPreview,
                  this);
            }
            if (Settings::AutoAdvance::get() && !m_client->isPlaying())
              recordInput(Input::Event::Skip{1});
          },
          [this](const Input::Event::Off &e) {
            m_client->changeEditState(
                [&](EditState &state) {
                  int key =
                      Settings::PolyphonicMidiNotePreview::get() ? e.key : -1;
                  m_record_note_preview[key].reset();

                  if (state.m_input_state.has_value()) {
                    Input::State::On &v = state.m_input_state.value();
                    if (v.on.key == e.key) {
                      applyOn(v, m_moo_clock->nowNoWrap(), m_client);
                      state.m_input_state.reset();
                    }
                  }
                },
                false);
          },
          [this](const Input::Event::Skip &s) {
            int end =
                quantize(m_moo_clock->nowNoWrap(), m_client->quantizeClock()) +
                m_client->quantizeClock() * s.offset;
            m_keyboard_view->ensurePlayheadFollowed();
            m_client->seekMoo(end);
          },
      },
      e);
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
      return save();
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
void EditorWindow::Host(HostSetting host_setting) {
  if (!maybeSave()) return;
  if (m_server) {
    int other_sessions = 0;
    for (const auto *s : m_server->sessions())
      if (s->uid() != m_client->uid()) ++other_sessions;
    if (other_sessions > 0) {
      auto result =
          QMessageBox::question(this, tr("Others connected to server"),
                                tr("There are other users (%n) connected to "
                                   "the server. Stop the server and "
                                   "start a new one?",
                                   "", other_sessions));
      if (result != QMessageBox::Yes) return;
    }
  }
  switch (host_setting) {
    case HostSetting::NewFile:
      if (!m_host_dialog->start(false)) return;
      break;
    case HostSetting::LoadFile:
      if (!m_host_dialog->start(true)) return;
      break;
    case HostSetting::SkipFile:
      if (!m_host_dialog->exec()) return;
      break;
  }
  m_host_dialog->persistSettings();

  std::optional<QString> filename = m_host_dialog->projectName();
  QString username = m_host_dialog->username();
  QHostAddress host = QHostAddress::LocalHost;
  int port = 0;
  if (m_host_dialog->port().has_value()) {
    bool ok;
    int p = m_host_dialog->port().value().toInt(&ok);
    if (!ok) {
      QMessageBox::warning(this, tr("Invalid port"), tr("Invalid port"));
      return;
    }
    port = p;
    host = QHostAddress::Any;
  }

  if (m_server) {
    m_client->disconnectFromServerSuppressSignal();
    delete m_server;
    m_server = nullptr;
    m_connection_status->setServerConnectionState(std::nullopt);
    qDebug() << "Stopped old server";
  }

  std::optional<QString> recording_save_file = m_host_dialog->recordingName();

  hostDirectly(filename, host, port, recording_save_file, username);
}

bool EditorWindow::event(QEvent *event) {
  if (event->type() == InputEvent::registeredType()) {
    InputEvent *myEvent = static_cast<InputEvent *>(event);
    recordInput(myEvent->event);
    return true;
  }
  return QWidget::event(event);
}

void EditorWindow::setCurrentFilename(std::optional<QString> filename) {
  m_filename = filename;
  setWindowTitle("pxtone collab - " +
                 QFileInfo(m_filename.value_or("New")).fileName());
}

void EditorWindow::hostDirectly(std::optional<QString> filename,
                                QHostAddress host, int port,
                                std::optional<QString> recording_save_file,
                                QString username) {
  try {
    m_server = new BroadcastServer(filename, host, port, recording_save_file,
                                   this);  // , 3000, 0.3);
  } catch (QString e) {
    QMessageBox::critical(this, "Server startup error", e);
    return;
  }
  QString window_title_filename;
  m_connection_status->setServerConnectionState(
      QString("%1:%2")
          .arg(m_server->address().toString())
          .arg(m_server->port()));
  setCurrentFilename(m_server->isReadingHistory() ? std::nullopt : filename);
  m_modified = false;
  m_side_menu->setModified(false);

  m_client->connectToLocalServer(m_server, username);
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
    QMessageBox::warning(
        this, tr("Could not save file"),
        tr("Could not open file %1 for writing").arg(filename));
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
bool EditorWindow::saveAs() {
  QSettings settings;
  QString filename = QFileDialog::getSaveFileName(
      this, "Save file",
      QFileInfo(settings.value(PTCOP_FILE_KEY).toString()).absolutePath(),
      "pxtone projects (*.ptcop)");
  if (filename.isEmpty()) return false;

  settings.setValue(PTCOP_FILE_KEY, QFileInfo(filename).absoluteFilePath());

  if (QFileInfo(filename).suffix() != "ptcop") filename += ".ptcop";
  bool saved = saveToFile(filename);
  if (saved) setCurrentFilename(filename);
  return saved;
}

bool EditorWindow::render() {
  double length, fadeout, volume;
  double secs_per_meas =
      m_pxtn.master->get_beat_num() / m_pxtn.master->get_beat_tempo() * 60;
  const pxtnMaster *m = m_pxtn.master;
  m_render_dialog->setSongLength(m->get_play_meas() * secs_per_meas);
  m_render_dialog->setSongLoopLength(
      (m->get_play_meas() - m->get_repeat_meas()) * secs_per_meas);
  m_render_dialog->setVolume(m_client->moo()->params.master_vol);

  try {
    if (!m_render_dialog->exec()) return false;
    length = m_render_dialog->renderLength();
    fadeout = m_render_dialog->renderFadeout();
    volume = m_render_dialog->renderVolume();
  } catch (QString &e) {
    QMessageBox::warning(this, tr("Render settings invalid"), e);
    return false;
  }

  if (QFileInfo(m_render_dialog->renderDestination()).suffix() != "wav") {
    QMessageBox::warning(this, tr("Could not render"),
                         tr("Filename must end with .wav"));
    return false;
  }

  QSaveFile file(m_render_dialog->renderDestination());
  if (!file.open(QIODevice::WriteOnly)) {
    QMessageBox::warning(this, tr("Could not render"),
                         tr("Could not open file for rendering"));
    return false;
  }

  constexpr int GRANULARITY = 1000;
  QProgressDialog progress(tr("Rendering"), tr("Abort"), 0, GRANULARITY, this);
  progress.setWindowModality(Qt::WindowModal);
  bool result = m_client->controller()->render(
      &file, length, fadeout, volume, [&](double p) {
        progress.setValue(p * GRANULARITY);
        return !progress.wasCanceled();
      });
  progress.close();
  if (!result) return false;
  file.commit();
  QMessageBox::information(this, tr("Rendering done"), tr("Rendering done"));
  return true;
}

bool EditorWindow::save() {
  if (!m_filename.has_value())
    return saveAs();
  else
    return saveToFile(m_filename.value());
}

void EditorWindow::connectToHost() {
  if (!maybeSave()) return;
  if (m_server &&
      QMessageBox::question(this, "Server running", "Stop the server first?") !=
          QMessageBox::Yes)
    return;

  if (!m_connect_dialog->exec()) return;
  m_connect_dialog->persistSettings();
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

void EditorWindow::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void EditorWindow::dropEvent(QDropEvent *event) {
  auto unsupported = [this]() {
    QMessageBox::warning(
        this, tr("Unsupported file type"),
        tr("Unsupported file type. Must be local ptcop or instruments."));
  };
  if (!event->mimeData()->hasUrls()) {
    unsupported();
    return;
  }
  for (QUrl url : event->mimeData()->urls()) {
    if (!url.isLocalFile()) {
      unsupported();
      return;
    }
  }
  QString project = "";
  if (event->mimeData()->urls().length() == 1) {
    QString file = event->mimeData()->urls().at(0).toLocalFile();
    if (QStringList{"ptcop", "ptrec"}.contains(QFileInfo(file).suffix())) {
      m_host_dialog->setProjectName(
          event->mimeData()->urls().at(0).toLocalFile());
      Host(HostSetting::SkipFile);
      return;
    }
  }

  std::list<AddWoice> add_woices;
  try {
    for (QUrl url : event->mimeData()->urls())
      add_woices.push_back(make_addWoice_from_path(url.toLocalFile()));
  } catch (QString &e) {
    QMessageBox::warning(this, tr("Unsupported instrument type"), e);
    return;
  }
  for (const AddWoice &w : add_woices) m_client->sendAction(w);
}
