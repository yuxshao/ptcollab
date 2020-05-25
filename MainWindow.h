#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFile>
#include <QtMultimedia/QAudioOutput>
#include "pxtone/pxtnService.h"
#include "PxtoneIODevice.h"

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
    void loadFile();

private:
    QFile sourceFile;
    QAudioOutput* audio;
    pxtnService m_pxtn;
    PxtoneIODevice m_pxtn_device;

    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
