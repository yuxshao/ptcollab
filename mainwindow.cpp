#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioDeviceInfo>
#include <QtMultimedia/QAudioOutput>
#include <QDebug>
#include <QFileDialog>
#include "pxtone/pxtnDescriptor.h"
#include <cstdio>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_pxtn_device(&m_pxtn)
    , ui(new Ui::MainWindow)
{
    m_pxtn.init();
    int channel_num = 2;
    int sample_rate = 44100;
    m_pxtn.set_destination_quality(channel_num, sample_rate);
    ui->setupUi(this);
    sourceFile.setFileName("/tmp/a.raw");
    sourceFile.open(QIODevice::ReadOnly);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::loadFile);

    QAudioFormat format;
    // Set up the format, eg.
    format.setSampleRate(sample_rate);
    format.setChannelCount(channel_num);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(format)) {
        qWarning() << "Raw audio format not supported by backend, cannot play audio.";
        return;
    }

    audio = new QAudioOutput(format, this);
    connect(audio, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State)));
    audio->setVolume(0.1);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::loadFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open file", "/home/steven/Projects/Music/pxtone/my_project", "pxtone projects (*.ptcop)");
    FILE *f = fopen(fileName.toStdString().c_str(), "r");
    pxtnDescriptor desc;
    desc.set_file_r(f);
    if (m_pxtn.read(&desc) != pxtnOK) { qWarning() << "Error reading file"; }
    if (m_pxtn.tones_ready() != pxtnOK) { qWarning() << "Error getting tones ready"; }
    pxtnVOMITPREPARATION prep{0};
    prep.flags          |= pxtnVOMITPREPFLAG_loop;
    prep.start_pos_float =     0;
    prep.master_volume   = 0.80f;

    if( !m_pxtn.moo_preparation( &prep ) ) { qWarning() << "Moo preparation error"; }
    fclose(f);

    m_pxtn_device.open(QIODevice::ReadOnly);
    audio->start(&m_pxtn_device);
}
