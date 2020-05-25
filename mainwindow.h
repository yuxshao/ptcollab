#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFile>
#include <QtMultimedia/QAudioOutput>
#include "pxtone/pxtnService.h"
#include "pxtoneiodevice.h"

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
    Ui::MainWindow *ui;

    QFile sourceFile;   // class member.
    QAudioOutput* audio; // class member.
    pxtnService m_pxtn;
    PxtoneIODevice m_pxtn_device;
};
#endif // MAINWINDOW_H
