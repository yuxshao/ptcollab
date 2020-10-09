#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFile>
#include <QLabel>
#include <QMainWindow>
#include <QScrollArea>
#include <QSplitter>

#include "EditState.h"
#include "EditorScrollArea.h"
#include "HostDialog.h"
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

class EditorWindow : public QMainWindow {
  Q_OBJECT

 public:
  EditorWindow(QWidget* parent = nullptr);
  ~EditorWindow();

 private slots:
  void connectToHost();

 private:
  void Host(bool load_file);
  void keyPressEvent(QKeyEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  KeyboardView* m_keyboard_view;
  pxtnService m_pxtn;
  mooState m_moo_state;
  EditorScrollArea *m_scroll_area, *m_param_scroll_area, *m_measure_scroll_area;
  QSplitter* m_splitter;
  QSplitter* m_key_splitter;
  QFrame* m_measure_splitter;
  PxtoneSideMenu* m_side_menu;
  BroadcastServer* m_server;
  PxtoneClient* m_client;
  MooClock* m_moo_clock;
  std::optional<QString> m_filename;
  QLabel *m_server_status, *m_client_status, *m_fps_status;
  bool m_modified;
  HostDialog* m_host_dialog;

  Ui::EditorWindow* ui;
  bool saveToFile(QString filename);
  void save();
  void saveAs();
  bool maybeSave();
};
#endif  // MAINWINDOW_H
