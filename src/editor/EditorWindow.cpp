#include "EditorWindow.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QSaveFile>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QtMultimedia/QAudioDeviceInfo>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>

#include "ComboOptions.h"
#include "InputEvent.h"
#include "Settings.h"
#include "WelcomeDialog.h"
#include "editor/StyleEditor.h"
#include "pxtone/pxtnDescriptor.h"
#include "ui_EditorWindow.h"
#include "views/MeasureView.h"
#include "views/MooClock.h"
#include "views/ParamView.h"
#include "views/ViewHelper.h"

// TODO: Maybe we could not hard-code this and change the engine to be dynamic
// w/ smart pointers.

#undef EVENT_MAX  // winuser.h conflict -- warns without
static constexpr int EVENT_MAX = 1000000;

static constexpr int AUTOSAVE_CHECK_INTERVAL_MS = 1 * 1000;
static constexpr int AUTOSAVE_WRITE_PERIOD = 30;

QString autoSaveDir() {
  return QStandardPaths::writableLocation(
             QStandardPaths::AppLocalDataLocation) +
         "/autosave/";
}

QString autoSavePath(const QString &filename) {
  return QDir(autoSaveDir()).filePath(filename);
}

EditorWindow::EditorWindow(QWidget *parent)
    : QMainWindow(parent),
      m_server(nullptr),
      m_filename(std::nullopt),
      m_connection_status(new ConnectionStatusLabel(this)),
      m_fps_status(new QLabel("FPS", this)),
      m_ping_status(new QLabel("", this)),
      m_modified(false),
      m_modified_autosave(false),
      m_autosave_counter(0),
      m_autosave_timer(new QTimer(this)),
      m_host_dialog(new HostDialog(this)),
      m_welcome_dialog(new WelcomeDialog(this)),
      m_connect_dialog(new ConnectDialog(this)),
      m_shortcuts_dialog(new ShortcutsDialog(this)),
      m_render_dialog(new RenderDialog(this)),
      m_midi_wrapper(new MidiWrapper()),
      m_settings_dialog(new SettingsDialog(m_midi_wrapper, this)),
      m_copy_options_dialog(nullptr),
      m_new_woice_dialog(nullptr),
      m_change_woice_dialog(nullptr),
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
  m_new_woice_dialog = new NewWoiceDialog(true, m_client, this);
  m_change_woice_dialog = new NewWoiceDialog(false, m_client, this);

  statusBar()->addPermanentWidget(m_fps_status);
  statusBar()->addPermanentWidget(m_ping_status);
  statusBar()->addPermanentWidget(m_connection_status);

  m_side_menu = new PxtoneSideMenu(m_client, m_moo_clock, m_new_woice_dialog,
                                   m_change_woice_dialog, this);
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
  m_keyboard_view = new KeyboardView(m_client, m_scroll_area);
  m_scroll_area->setWidget(m_keyboard_view);

  // TODO: find a better place for this.
  connect(m_keyboard_view, &KeyboardView::setScrollOnClick, m_scroll_area,
          &EditorScrollArea::setEnableScrollWithMouseX);
  connect(m_keyboard_view, &KeyboardView::ensureVisibleX,
          [this](int x, bool strict) {
            if (!strict)
              m_scroll_area->ensureWithinMargin(x, -0.02, 0.15, 0.15, 0.75);
            else
              m_scroll_area->ensureWithinMargin(x, 0.45, 0.45, 0.45, 0.45);
          });
  connect(m_scroll_area, &EditorScrollArea::viewportChanged,
          [this](const QRect &viewport) {
            m_client->changeEditState(
                [&](EditState &e) {
                  // Logical coords of just the roll area w/o the left piano
                  e.viewport = worldTransform().inverted().mapRect(viewport);
                  e.viewport.adjust(Settings::LeftPianoWidth::get(), 0, 0, 0);
                },
                true);
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
          // Logical coords of roll area with left piano
          QPointF logical_pos{
              startClock / e.scale.clockPerPx - Settings::LeftPianoWidth::get(),
              startPitch / e.scale.pitchPerPx};
          // Scroll coords of roll area with left piano
          QPointF scroll_pos = worldTransform().map(logical_pos);
          m_scroll_area->horizontalScrollBar()->setValue(scroll_pos.x());
          m_scroll_area->verticalScrollBar()->setValue(scroll_pos.y());
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
  m_param_scroll_area->setWidget(new ParamView(m_client, m_param_scroll_area));

  m_measure_scroll_area = new EditorScrollArea(m_splitter, false);
  m_measure_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_measure_scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_measure_view = new MeasureView(m_client, m_measure_scroll_area);
  m_measure_scroll_area->setWidget(m_measure_view);
  m_measure_scroll_area->setSizePolicy(
      QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
  connect(m_measure_view, &MeasureView::heightChanged, this, [this](int h) {
    // This is a bit jank, but it seems like changing the inner content material
    // isn't enough for changing how high this widget is in the scrollarea.
    m_measure_scroll_area->setMinimumHeight(h);
    m_measure_scroll_area->setMaximumHeight(h);
  });
  m_measure_scroll_area->setMaximumHeight(
      m_measure_scroll_area->widget()->sizeHint().height());
  connect(m_side_menu, &SideMenu::hoveredUnitChanged, m_measure_view,
          &MeasureView::setFocusedUnit);
  connect(m_side_menu, &SideMenu::hoveredUnitChanged, m_keyboard_view,
          &KeyboardView::setFocusedUnit);
  // Below is just so that if you change units with W/S while hovered, the
  // highlighted unit isn't hijacked by your current mouse position.
  connect(m_side_menu, &SideMenu::currentUnitChanged, m_keyboard_view,
          [this](int) { m_keyboard_view->setFocusedUnit(std::nullopt); });

  connect(m_measure_view, &MeasureView::hoverUnitNoChanged, m_side_menu,
          [this](std::optional<int> unit_no, bool) {
            m_side_menu->setFocusedUnit(unit_no);
          });
  connect(m_measure_view, &MeasureView::hoverUnitNoChanged, m_keyboard_view,
          [this](std::optional<int> unit_no, bool selecting_unit) {
            m_keyboard_view->setFocusedUnit(selecting_unit ? unit_no
                                                           : std::nullopt);
          });
  connect(m_keyboard_view, &KeyboardView::hoverUnitNoChanged, m_measure_view,
          &MeasureView::setFocusedUnit);
  connect(m_keyboard_view, &KeyboardView::hoverUnitNoChanged, m_side_menu,
          &SideMenu::setFocusedUnit);

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

  connect(m_side_menu, &SideMenu::saveButtonPressed, [this]() { save(false); });
  connect(ui->actionSave, &QAction::triggered, [this]() { save(false); });

  connect(m_client->controller(), &PxtoneController::edited, [this]() {
    m_modified = true;
    m_modified_autosave = true;
    m_side_menu->setModified(true);
  });

  connect(ui->actionNewHost, &QAction::triggered,
          [this]() { Host(HostSetting::NewFile); });
  connect(ui->actionOpenHost, &QAction::triggered,
          [this]() { Host(HostSetting::LoadFile); });
  connect(ui->actionSaveAs, &QAction::triggered, [this]() { save(true); });
  connect(ui->actionRender, &QAction::triggered, this, &EditorWindow::render);
  connect(ui->actionConnect, &QAction::triggered, this,
          &EditorWindow::connectToHost);
  connect(ui->actionClear_Settings, &QAction::triggered, [this]() {
    if (QMessageBox::question(this, tr("Clear settings"),
                              tr("Are you sure you want to clear your app "
                                 "settings?")))
      Settings::clear();
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
          [this](int port_no, const QString &name) {
            qDebug() << "using midi port" << port_no << name;
            if (m_midi_wrapper->usePort(port_no, [this](Input::Event::Event x) {
                  QCoreApplication::postEvent(this, new InputEvent(x),
                                              Qt::HighEventPriority);
                }))
              Settings::AutoConnectMidiName::set(name);
          });
  if (Settings::AutoConnectMidi::get()) {
    int port_no =
        m_midi_wrapper->ports().indexOf(Settings::AutoConnectMidiName::get());

    qDebug() << "using startup midi port" << port_no;
    m_midi_wrapper->usePort(port_no, [this](Input::Event::Event x) {
      QCoreApplication::postEvent(this, new InputEvent(x),
                                  Qt::HighEventPriority);
    });
  }

  connect(m_settings_dialog, &SettingsDialog::quantYOptionsChanged, this,
          [this]() {
            m_side_menu->updateQuantizeYOptions(
                m_client->editState().m_quantize_pitch_denom);
          });
  connect(m_settings_dialog, &SettingsDialog::accepted, m_side_menu,
          &SideMenu::refreshVolumeMeterShowText);
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

  m_autosave_timer->start(AUTOSAVE_CHECK_INTERVAL_MS);
  m_autosave_filename =
      "session-" +
      QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss") + ".ptcop";
  connect(m_autosave_timer, &QTimer::timeout, this, &EditorWindow::autoSave);
  checkForOldAutoSaves();

  if (Settings::ShowWelcomeDialog::get()) {
    // In a timer so that the main window has time to show up
    QTimer::singleShot(0, m_welcome_dialog, &QDialog::exec);
  }
}

EditorWindow::~EditorWindow() {
  // https://github.com/yuxshao/ptcollab/issues/56
  // This deletion is so that m_moo_clock is deleted before pxtnService, which
  // reads as part of an animation tick, is deleted.
  delete m_moo_clock;

  delete ui;
}

// #define DEBUG_RECORD_INPUT

void EditorWindow::keyReleaseEvent(QKeyEvent *event) {
  if (!event->isAutoRepeat()) {
    int key = event->key();
    switch (key) {
      case Qt::Key::Key_R:
        rKeyStateChanged(false);
        break;
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
      else if (event->modifiers() & Qt::ShiftModifier) {
        EVENTKIND kind =
            paramOptions()[m_client->editState().current_param_kind_idx()]
                .second;
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
                                      ? (Settings::StrictFollowSeek::get()
                                             ? FollowPlayhead::Follow
                                             : FollowPlayhead::Jump)
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
            if (qx < int(quantizeXOptions().size()) - 1) qx++;
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
          m_client->cycleSolo(unit_no);
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
#else
      if (event->modifiers() & Qt::ShiftModifier)
        m_side_menu->openAddUnitWindow();
      else
        m_new_woice_dialog->show();
#endif
      break;
    case Qt::Key_P:
      if (event->modifiers() & Qt::ControlModifier)
        Settings::ChordPreview::set(!Settings::ChordPreview::get());
      break;
    case Qt::Key_Q:
      if (event->modifiers() & Qt::AltModifier) {
        if (event->modifiers() & Qt::ShiftModifier)
          m_keyboard_view->quantizeSelectionY();
        else
          m_keyboard_view->quantizeSelectionX();
      }
      break;
    case Qt::Key::Key_R:
      if (event->modifiers() & Qt::ControlModifier && !event->isAutoRepeat()) {
#ifdef QT_DEBUG
        if (event->modifiers() & Qt::ShiftModifier)
          StyleEditor::tryLoadStyle(Settings::StyleName::get());
        else
          Settings::RecordMidi::set(!Settings::RecordMidi::get());
#else
        Settings::RecordMidi::set(!Settings::RecordMidi::get());
#endif
      } else {
        rKeyStateChanged(true);
      }
      break;
    case Qt::Key_S:
      if (!(event->modifiers() & Qt::ControlModifier))
        m_keyboard_view->cycleCurrentUnit(
            1, event->modifiers() & Qt::ShiftModifier);
      break;
    case Qt::Key_W:
      m_keyboard_view->cycleCurrentUnit(-1,
                                        event->modifiers() & Qt::ShiftModifier);
      break;
    case Qt::Key_V:
      if (event->modifiers() & Qt::ControlModifier)
        m_keyboard_view->paste((event->modifiers() & Qt::ShiftModifier), false);
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
      m_keyboard_view->cycleCurrentUnit(1,
                                        event->modifiers() & Qt::ShiftModifier);
      break;
    case Qt::Key_PageUp:
      m_keyboard_view->cycleCurrentUnit(-1,
                                        event->modifiers() & Qt::ShiftModifier);
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
    case Qt::Key_Right: {
      bool shift_mod = (event->modifiers() & Qt::ShiftModifier);
      bool shift_right = event->key() == Qt::Key_Right;
      if (event->modifiers() & Qt::AltModifier) {
        tweakSelectionRange(shift_right, shift_mod);
      } else if (shift_mod) {
        m_client->changeEditState(
            [&](auto &s) {
              if (!s.mouse_edit_state.selection.has_value()) return;
              Interval &sel = s.mouse_edit_state.selection.value();
              qint32 q;
              if (event->modifiers() & Qt::ControlModifier)
                q = m_pxtn.master->get_beat_num() *
                    m_pxtn.master->get_beat_clock();
              else
                q = m_client->quantizeClock();
              int proposed_from_end, proposed_from_start;
              if (shift_right) {
                proposed_from_end = quantize(sel.end + q, q) - sel.end;
                proposed_from_start = quantize(sel.start + q, q) - sel.start;
              } else {
                proposed_from_end = quantize(sel.end - 1, q) - sel.end;
                proposed_from_start = quantize(sel.start - 1, q) - sel.start;
              }

              int shift = proposed_from_start;
              if (abs(proposed_from_end) < abs(proposed_from_start))
                shift = proposed_from_end;
              auto [actions, length] = m_client->clipboard()->makeShift(
                  m_client->selectedUnitNos(), sel, sel.start + shift,
                  m_client->pxtn(), m_client->controller()->woiceIdMap(),
                  m_client->controller()->unitIdMap());

              Interval difference;
              if (shift_right)
                difference = {sel.start, sel.start + shift};
              else
                difference = {sel.end + shift, sel.end};
              if (!difference.empty())
                actions.splice(actions.end(),
                               m_client->clipboard()->makeClear(
                                   m_client->selectedUnitNos(), difference,
                                   m_client->controller()->unitIdMap()));
              if (actions.size() > 0) m_client->applyAction(actions);
              sel.start += shift;
              sel.end += shift;
            },
            false);
      }
    } break;
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

void EditorWindow::rKeyStateChanged(bool state) {
  m_measure_view->setSelectUnitEnabled(state);
  m_keyboard_view->setSelectUnitEnabled(state);
}

void applyOn(const Input::State::On &v, int unit_id, int end,
             PxtoneClient *client) {
  std::vector<Interval> clock_ints = v.clock_ints(end, client->pxtn()->master);
  // TODO: Dedup
  using namespace Action;
  std::list<Primitive> actions;
  for (const auto &clock_int : clock_ints) {
    actions.push_back(
        {EVENTKIND_ON, unit_id, clock_int.start, Delete{clock_int.end}});
    actions.push_back(
        {EVENTKIND_VELOCITY, unit_id, clock_int.start, Delete{clock_int.end}});
    actions.push_back(
        {EVENTKIND_KEY, unit_id, clock_int.start, Delete{clock_int.end}});
    actions.push_back(
        {EVENTKIND_ON, unit_id, clock_int.start, Add{clock_int.length()}});
    actions.push_back(
        {EVENTKIND_VELOCITY, unit_id, clock_int.start, Add{v.on.vel()}});
    actions.push_back({EVENTKIND_KEY, unit_id, clock_int.start, Add{v.on.key}});
  }
  if (actions.size() > 0) client->applyAction(actions);
}

struct FinalizedMidiNote {
  Input::State::On details;
  int unit_id;
};

std::vector<FinalizedMidiNote> receiveOnEvent(
    const Input::Event::On &e, int now, Input::State::State &state,
    const std::set<int> &selected_unit_ids) {
  std::vector<FinalizedMidiNote> finalized;
  // take out all things not currently selected
  for (auto it = state.notes_by_id.begin(); it != state.notes_by_id.end();) {
    if (selected_unit_ids.count(it->first) == 0) {
      finalized.push_back({it->second, it->first});
      it = state.notes_by_id.erase(it);
    } else
      ++it;
  }
  // if it's full take out the first input note
  if (state.notes_by_id.size() == selected_unit_ids.size()) {
    auto it = state.notes_by_id.begin();
    auto min = state.notes_by_id.begin();
    while (it != state.notes_by_id.end()) {
      if (it->second.start_clock < min->second.start_clock) min = it;
      ++it;
    }
    if (min != state.notes_by_id.end()) {
      finalized.push_back({min->second, min->first});
      state.notes_by_id.erase(min);
    }
  }
  // if there's more than 1 empty space put it in the lowest one (or the one
  // whose last note was closest?)
  for (auto i : selected_unit_ids) {
    if (state.notes_by_id.count(i) == 0) {
      state.notes_by_id[i] = Input::State::On{now, e};
      break;
    }
  }
  return finalized;
}
std::vector<FinalizedMidiNote> receiveOffEvent(
    const Input::Event::Off &e, Input::State::State &state,
    const std::set<int> &selected_unit_ids) {
  std::vector<FinalizedMidiNote> finalized;
  // take out all things not currently selected + the thing that was just turned
  // off
  for (auto it = state.notes_by_id.begin(); it != state.notes_by_id.end();) {
    if (selected_unit_ids.count(it->first) == 0 || it->second.on.key == e.key) {
      finalized.push_back({it->second, it->first});
      it = state.notes_by_id.erase(it);
    } else
      ++it;
  }
  return finalized;
}

void EditorWindow::recordInput(const Input::Event::Event &e) {
  if (m_new_woice_dialog->isVisible()) {
    m_new_woice_dialog->inputMidi(e);
    return;
  }
  if (m_change_woice_dialog->isVisible()) {
    m_change_woice_dialog->inputMidi(e);
    return;
  }
  std::visit(
      overloaded{
          [this](const Input::Event::On &e) {
            // TODO: handle repeat
            int start = m_moo_clock->nowNoWrap();
            if (Settings::RecordMidi::get()) {
              m_client->changeEditState(
                  [&](EditState &state) {
                    for (const FinalizedMidiNote &a :
                         receiveOnEvent(e, start, state.m_input_state,
                                        m_client->selectedUnitIds()))
                      applyOn(a.details, a.unit_id, start, m_client);
                  },
                  false);
            }
            auto maybe_unit_no = m_client->unitIdMap().idToNo(
                m_client->editState().m_current_unit_id);
            if (maybe_unit_no != std::nullopt) {
              qint32 unit_no = maybe_unit_no.value();
              if (!Settings::PolyphonicMidiNotePreview::get()) {
                m_record_note_preview.clear();
                m_keyboard_view->currentMidiNotes().clear();
              }
              bool chordPreview =
                  (!Settings::PolyphonicMidiNotePreview::get()) &&
                  Settings::ChordPreview::get() && !m_client->isPlaying();
              m_record_note_preview[e.key] = std::make_unique<NotePreview>(
                  &m_pxtn, &m_client->moo()->params, unit_no, start, e.key,
                  e.vel(), m_client->bufferSize(), chordPreview, this);

              m_keyboard_view->currentMidiNotes()[e.key] = e.vel();
            }
            if (Settings::AutoAdvance::get() && Settings::RecordMidi::get() &&
                !m_client->isPlaying())
              recordInput(Input::Event::Skip{1});
          },
          [this](const Input::Event::Off &e) {
            m_client->changeEditState(
                [&](EditState &state) {
                  m_record_note_preview[e.key].reset();
                  m_keyboard_view->currentMidiNotes().erase(e.key);

                  for (const FinalizedMidiNote &a : receiveOffEvent(
                           e, state.m_input_state, m_client->selectedUnitIds()))
                    applyOn(a.details, a.unit_id, m_moo_clock->nowNoWrap(),
                            m_client);
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
      return save(false);
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

void EditorWindow::autoSave() {
  QString path = autoSavePath(m_autosave_filename);
  QDir dir(autoSaveDir());
  ++m_autosave_counter;

  if (!m_modified_autosave ||
      (m_autosave_counter % AUTOSAVE_WRITE_PERIOD) > 0) {
    // update file modification time to signal it's still active
    QFile file(path);
    if (file.exists()) {
      if (file.open(QIODevice::ReadWrite)) {
        file.setFileTime(QDateTime::currentDateTime(),
                         QFileDevice::FileModificationTime);
      } else
        qWarning() << "Unable to update autosave file modification time"
                   << path;
    }

    return;
  }

  m_autosave_counter = 0;
  dir.mkpath(dir.path());

  if (!saveToFile(path, false))
    qWarning() << "Unable to save file to autosave location" << path;
  else {
    m_modified_autosave = false;
    qDebug() << "Autosaved to " << path;
  }
}

void EditorWindow::checkForOldAutoSaves() {
  QDirIterator it(autoSaveDir(), {"*.ptcop"});
  while (it.hasNext()) {
    QFileInfo f(it.next());
    if (QDateTime::currentDateTime() >=
        f.lastModified().addMSecs(3 * AUTOSAVE_CHECK_INTERVAL_MS)) {
      auto result = QMessageBox::information(
          this, tr("Found backup files from previous run"),
          tr("Old backup save files found. This usually happens if a "
             "previous ptcollab session quit unexpxectedly. Opening backup "
             "folder. If you want to stop seeing this message, simply "
             "remove all the files from the folder (%1). ")
              .arg(autoSaveDir()));
      if (result == QMessageBox::Ok) {
        if (!QDesktopServices::openUrl(autoSaveDir()))
          QMessageBox::information(this, tr("Unable to open backup folder"),
                                   tr("Unable to open backup folder. You can "
                                      "access your backups at %1")
                                       .arg(autoSaveDir()));
      }
      break;
    }
  }
}

void EditorWindow::closeEvent(QCloseEvent *event) {
  if (maybeSave()) {
    qDebug() << "Deleting autosave file due to regular shutdown"
             << autoSavePath(m_autosave_filename);
    QFile::remove(autoSavePath(m_autosave_filename));

    event->accept();
  } else
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

bool EditorWindow::eventFilter(QObject *watched, QEvent *event) {
  switch (event->type()) {
    case QEvent::Show:
      // https://doc.qt.io/qt-5/qshowevent.html
      // Spontaneous QShowEvents are sent by the window system after the window
      // is shown, as well as when it's reshown after being iconified. We only
      // want this to trigger when Qt sends it, which is right before it becomes
      // visible.
      if (!event->spontaneous()) {
        QWidget *w = qobject_cast<QWidget *>(watched);
        if (w) StyleEditor::setTitleBar(w);
      }
      return false;
    default:
      return false;
  }
}

bool EditorWindow::saveToFile(QString filename, bool warnOnError) {
#ifdef _WIN32
  FILE *f_raw;
  _wfopen_s(&f_raw, filename.toStdWString().c_str(), L"wb");
#else
  FILE *f_raw = fopen(filename.toStdString().c_str(), "wb");
#endif
  std::unique_ptr<std::FILE, decltype(&fclose)> f(f_raw, &fclose);
  if (!f) {
    qWarning() << "Could not open file" << filename;
    if (warnOnError)
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
  return true;
}

bool EditorWindow::save(bool forceSelectFilename) {
  QString filename;
  if (m_filename.has_value() && !forceSelectFilename)
    filename = m_filename.value();
  else {
    QFileDialog f(this, "Save file", "", "pxtone projects (*.ptcop)");
    f.setAcceptMode(QFileDialog::AcceptSave);
    if (Settings::OpenProjectState::isSet()) {
      f.setDirectory("");  // Unset directory so the state also restores dir
      if (!f.restoreState(Settings::OpenProjectState::get()))
        qWarning() << "Failed to restore save dialog state";
    }
    if (f.exec() == QDialog::Accepted) {
      filename = f.selectedFiles().value(0);
      Settings::OpenProjectState::set(f.saveState());
    }
  }

  if (filename.isEmpty()) return false;

  if (QFileInfo(filename).suffix() != "ptcop") filename += ".ptcop";
  bool saved = saveToFile(filename);
  if (saved) {
    Settings::OpenProjectLastSelection::set(
        QFileInfo(filename).absoluteFilePath());
    setCurrentFilename(filename);
    m_modified = false;
    m_side_menu->setModified(false);
  }
  return saved;
}

static QRegularExpression forbidden_filename_character_matcher("[\\/\"]+");
void EditorWindow::render() {
  double length, fadeout, volume;
  bool separate_units;
  double secs_per_meas =
      m_pxtn.master->get_beat_num() / m_pxtn.master->get_beat_tempo() * 60;
  const pxtnMaster *m = m_pxtn.master;
  m_render_dialog->setSongLength(m->get_play_meas() * secs_per_meas);
  m_render_dialog->setSongLoopLength(
      (m->get_play_meas() - m->get_repeat_meas()) * secs_per_meas);
  m_render_dialog->setVolume(m_client->moo()->params.master_vol);

  try {
    if (!m_render_dialog->exec()) return;
    length = m_render_dialog->renderLength();
    fadeout = m_render_dialog->renderFadeout();
    volume = m_render_dialog->renderVolume();
    separate_units = m_render_dialog->renderUnitsSeparately();
  } catch (QString &e) {
    QMessageBox::warning(this, tr("Render settings invalid"), e);
    return;
  }

  std::vector<std::pair<QString, std::optional<int>>> filenames_and_units;
  if (!separate_units) {
    if (QFileInfo(m_render_dialog->renderDestination()).suffix() != "wav") {
      QMessageBox::warning(this, tr("Could not render"),
                           tr("Filename must end with .wav"));
      return;
    }
    filenames_and_units.push_back(
        {m_render_dialog->renderDestination(), std::nullopt});
  } else {
    QFileInfo dir(m_render_dialog->renderDestination());
    if (!dir.isDir()) {
      QMessageBox::warning(this, tr("Could not render"),
                           tr("Filename must be a directory"));
      return;
    }
    for (int unit_no = 0; unit_no < m_pxtn.Unit_Num(); ++unit_no) {
      QString unit_name = shift_jis_codec->toUnicode(
          m_pxtn.Unit_Get(unit_no)->get_name_buf_jis(nullptr));
      unit_name.remove(forbidden_filename_character_matcher);
      QString filename = QString("%1/%2_%3.wav")
                             .arg(dir.absoluteFilePath())
                             .arg(unit_no)
                             .arg(unit_name);
      filenames_and_units.push_back({filename, unit_no});
    }
  }
  constexpr int GRANULARITY = 1000;
  QProgressDialog progress(tr("Rendering"), tr("Abort"), 0, GRANULARITY, this);
  progress.setWindowModality(Qt::WindowModal);

  uint num_tasks = filenames_and_units.size();
  for (uint i = 0; i < num_tasks; ++i) {
    const auto &[filename, solo_unit] = filenames_and_units[i];
    QSaveFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
      QMessageBox::warning(this, tr("Could not render"),
                           tr("Could not open file for rendering"));
      return;
    }
    progress.setLabelText(tr("Rendering") +
                          QString(" (%1/%2)").arg(i + 1).arg(num_tasks));

    bool finished;
    try {
      finished = m_client->controller()->render_exn(
          &file, length, fadeout, volume, solo_unit, [&](double p) {
            progress.setValue((p + i) * GRANULARITY / num_tasks);
            return !progress.wasCanceled();
          });
    } catch (const QString &e) {
      QMessageBox::warning(this, tr("Render error"), e);
      finished = false;
    }
    if (!finished) return;
    file.commit();
  }
  progress.close();
  QMessageBox::information(this, tr("Rendering done"), tr("Rendering done"));
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

  setCurrentFilename(std::nullopt);
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
  uint woice_capacity =
      m_client->pxtn()->Woice_Max() - m_client->pxtn()->Woice_Num();
  try {
    for (QUrl url : event->mimeData()->urls()) {
      if (add_woices.size() >= woice_capacity) {
        QMessageBox::warning(this, tr("Voice add error"),
                             tr("You are trying to add too many voices (%1). "
                                "There is space for %2.")
                                 .arg(event->mimeData()->urls().length())
                                 .arg(woice_capacity));
        return;
      }
      add_woices.push_back(make_addWoice_from_path_exn(url.toLocalFile(), ""));
    }
  } catch (QString &e) {
    QMessageBox::warning(this, tr("Unsupported instrument type"), e);
    return;
  }
  for (const AddWoice &w : add_woices) m_client->sendAction(w);
}
