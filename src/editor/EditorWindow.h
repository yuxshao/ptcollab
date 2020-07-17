#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFile>
#include <QLabel>
#include <QMainWindow>
#include <QScrollArea>
#include <QSplitter>
#include <QtMultimedia/QAudioOutput>

#include "EditState.h"
#include "EditorScrollArea.h"
#include "KeyboardView.h"
#include "PxtoneIODevice.h"
#include "PxtoneSideMenu.h"
#include "UnitListModel.h"
#include "pxtone/pxtnService.h"
#include "server/BroadcastServer.h"
#include "server/Client.h"
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
  void keyPressEvent(QKeyEvent* event);
  KeyboardView* m_keyboard_view;
  pxtnService m_pxtn;
  mooState m_moo_state;
  EditorScrollArea* m_scroll_area;
  QSplitter* m_splitter;
  PxtoneSideMenu* m_side_menu;
  BroadcastServer* m_server;
  PxtoneClient* m_client;
  QString m_filename;
  QLabel* m_server_status;
  QLabel* m_client_status;
  UnitListModel* m_units;

  Ui::EditorWindow* ui;
  bool saveToFile(QString filename);
  void save();
  void saveAs();
};
#endif  // MAINWINDOW_H
