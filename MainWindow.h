#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScrollArea>
#include <QFile>
#include <QtMultimedia/QAudioOutput>
#include "pxtone/pxtnService.h"
#include "PxtoneIODevice.h"
#include "KeyboardEditor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void selectAndLoadFile();

private:
    void loadFile(QString filename);
    QAudioOutput* m_audio;
    KeyboardEditor* m_keyboard_editor;
    pxtnService m_pxtn;
    QScrollArea* m_scroll_area;
    PxtoneIODevice m_pxtn_device;

    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
