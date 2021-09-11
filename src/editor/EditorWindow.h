#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFile>
#include <QLabel>
#include <QMainWindow>
#include <QScrollArea>
#include <QSplitter>

#include "ConnectDialog.h"
#include "ConnectionStatusLabel.h"
#include "CopyOptionsDialog.h"
#include "EditState.h"
#include "EditorScrollArea.h"
#include "HostDialog.h"
#include "RenderDialog.h"
#include "SettingsDialog.h"
#include "ShortcutsDialog.h"
#include "WelcomeDialog.h"
#include "audio/PxtoneIODevice.h"
#include "network/BroadcastServer.h"
#include "network/Client.h"
#include "pxtone/pxtnService.h"
#include "sidemenu/DelayEffectModel.h"
#include "sidemenu/PxtoneSideMenu.h"
#include "sidemenu/UnitListModel.h"
#include "views/KeyboardView.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class EditorWindow;
}
QT_END_NAMESPACE

enum struct HostSetting { LoadFile, NewFile, SkipFile };

class EditorWindow : public QMainWindow {
  Q_OBJECT

 public:
  EditorWindow(QWidget* parent = nullptr);
  ~EditorWindow();

  void hostDirectly(std::optional<QString> filename, QHostAddress host,
                    int port, std::optional<QString> recording_save_file,
                    QString username);
 private slots:
  void connectToHost();

 private:
  void Host(HostSetting host_setting);
  bool event(QEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  KeyboardView* m_keyboard_view;
  pxtnService m_pxtn;
  EditorScrollArea *m_scroll_area, *m_param_scroll_area, *m_measure_scroll_area;
  QSplitter* m_splitter;
  QSplitter* m_key_splitter;
  QFrame* m_measure_splitter;
  PxtoneSideMenu* m_side_menu;
  BroadcastServer* m_server;
  PxtoneClient* m_client;
  MooClock* m_moo_clock;
  std::optional<QString> m_filename;
  ConnectionStatusLabel* m_connection_status;
  QLabel *m_fps_status, *m_ping_status;
  bool m_modified;
  HostDialog* m_host_dialog;
  WelcomeDialog* m_welcome_dialog;
  ConnectDialog* m_connect_dialog;
  ShortcutsDialog* m_shortcuts_dialog;
  RenderDialog* m_render_dialog;
  MidiWrapper* m_midi_wrapper;
  SettingsDialog* m_settings_dialog;
  CopyOptionsDialog* m_copy_options_dialog;

  Ui::EditorWindow* ui;
  bool saveToFile(QString filename);
  bool save();
  bool saveAs();
  bool render();
  bool maybeSave();
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  std::map<int, std::unique_ptr<NotePreview>> m_record_note_preview;
  void recordInput(const Input::Event::Event& e);
  void setCurrentFilename(std::optional<QString> filename);
  void tweakSelectionRange(bool shift_right, bool grow);
};
#endif  // MAINWINDOW_H
