#include "MainWindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QtMultimedia/QAudioDeviceInfo>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>
#include <cstdio>

#include "pxtone/pxtnDescriptor.h"
#include "ui_MainWindow.h"

// TODO: Maybe we could not hard-code this and change the engine to be dynamic
// w/ smart pointers.
static constexpr int EVENT_MAX = 1000000;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_pxtn_device(this, &m_pxtn),
      ui(new Ui::MainWindow) {
  m_pxtn.init_collage(EVENT_MAX);
  int channel_num = 2;
  int sample_rate = 44100;
  m_pxtn.set_destination_quality(channel_num, sample_rate);
  ui->setupUi(this);
  connect(ui->actionOpen, &QAction::triggered, this,
          &MainWindow::selectAndLoadFile);

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
    qWarning()
        << "Raw audio format not supported by backend, cannot play audio.";
    return;
  }

  m_audio = new QAudioOutput(format, this);

  // m_audio->setBufferSize(441000);
  m_audio->setCategory(
      "game");  // Apparently this reduces latency in pulseaudio, but also makes
                // some sounds choppier
  m_audio->setVolume(0.5);
  loadFile("/home/steven/Projects/Music/pxtone/my_project/1353.ptcop");
  m_scroll_area = new EditorScrollArea(this);
  m_keyboard_editor = new KeyboardEditor(&m_pxtn, m_audio);

  m_scroll_area->setWidget(m_keyboard_editor);
  m_scroll_area->setBackgroundRole(QPalette::Dark);
  setCentralWidget(m_scroll_area);
  m_scroll_area->setVisible(true);
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
    case Qt::Key_Space:
      if (m_audio->state() == QAudio::SuspendedState)
        m_audio->resume();
      else
        m_audio->suspend();
      break;
    case Qt::Key_W:
      m_keyboard_editor->cycleCurrentUnit(-1);
      break;
    case Qt::Key_S:
      m_keyboard_editor->cycleCurrentUnit(1);
      break;
    case Qt::Key_A:
      m_keyboard_editor->toggleShowAllUnits();
      break;
  }
}

void MainWindow::loadFile(QString filename) {
  std::unique_ptr<std::FILE, decltype(&fclose)> f(
      fopen(filename.toStdString().c_str(), "r"), &fclose);
  if (!f) {
    qWarning() << "Could not open file";
    return;
  }
  pxtnDescriptor desc;
  desc.set_file_r(f.get());
  if (m_pxtn.read(&desc) != pxtnOK) {
    qWarning() << "Error reading file";
    return;
  }
  if (m_pxtn.tones_ready() != pxtnOK) {
    qWarning() << "Error getting tones ready";
    return;
  }
  pxtnVOMITPREPARATION prep{};
  prep.flags |= pxtnVOMITPREPFLAG_loop;
  prep.start_pos_float = 0;
  prep.master_volume = 0.80f;

  qDebug() << "Preparing moo for " << filename;
  bool success = m_pxtn.moo_preparation(&prep);
  if (!success) {
    qWarning() << "Moo preparation error";
    return;
  }

  m_pxtn_device.open(QIODevice::ReadOnly);
  m_audio->start(&m_pxtn_device);
  m_audio->suspend();
}

void MainWindow::selectAndLoadFile() {
  QString filename = QFileDialog::getOpenFileName(
      this, "Open file", "/home/steven/Projects/Music/pxtone/Examples/for-web",
      "pxtone projects (*.ptcop)");
  loadFile(filename);
}
