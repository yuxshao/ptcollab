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
#include "KeyboardEditor.h"
#include "PxtoneIODevice.h"
#include "SideMenu.h"
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
  void loadFileAndHost();

  void connectToHost();

 private:
  void loadFile(QString filename);
  void keyPressEvent(QKeyEvent* event);
  QAudioOutput* m_audio;
  KeyboardEditor* m_keyboard_editor;
  pxtnService m_pxtn;
  EditorScrollArea* m_scroll_area;
  PxtoneIODevice m_pxtn_device;
  QSplitter* m_splitter;
  SideMenu* m_side_menu;
  BroadcastServer* m_server;
  Client* m_client;
  QString m_filename;
  QLabel* m_server_status;
  QLabel* m_client_status;

  Ui::EditorWindow* ui;
  void togglePlayState();
  void resetAndSuspendAudio();
  bool saveToFile(QString filename);
  void save();
  void saveAs();
  bool loadDescriptor(pxtnDescriptor& desc);
};
#endif  // MAINWINDOW_H
