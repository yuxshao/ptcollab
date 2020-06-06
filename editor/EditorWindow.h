#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFile>
#include <QMainWindow>
#include <QScrollArea>
#include <QSplitter>
#include <QtMultimedia/QAudioOutput>

#include "EditorScrollArea.h"
#include "KeyboardEditor.h"
#include "PxtoneIODevice.h"
#include "SideMenu.h"
#include "pxtone/pxtnService.h"
#include "server/ActionClient.h"
#include "server/SequencingServer.h"
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
  SequencingServer* m_server;
  ActionClient* m_client;

  Ui::EditorWindow* ui;
  void togglePlayState();
  void resetAndSuspendAudio();
  void saveFile(QString filename);
  void selectAndSaveFile();
  bool loadDescriptor(pxtnDescriptor& desc);
};
#endif  // MAINWINDOW_H
