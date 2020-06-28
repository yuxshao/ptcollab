#include "NotePreview.h"

#include <QAudioFormat>
#include <QAudioOutput>

#include "NotePreview.h"
#include "PxtoneUnitIODevice.h"

NotePreview::NotePreview(const pxtnService *pxtn, int unit_no, int pitch,
                         int clock, int vel, QObject *parent)
    : QObject(parent),
      m_device(new PxtoneUnitIODevice(this, pxtn, unit_no, pitch, clock, vel)),
      m_audio(nullptr) {
  // TODO: Deduplicate this sample setup
  QAudioFormat format;
  int channel_num = 2;
  int sample_rate = 44100;
  format.setSampleRate(sample_rate);
  format.setChannelCount(channel_num);
  format.setSampleSize(16);
  format.setCodec("audio/pcm");
  format.setByteOrder(QAudioFormat::LittleEndian);
  format.setSampleType(QAudioFormat::SignedInt);
  m_audio = new QAudioOutput(format, m_device);

  m_device->open(QIODevice::ReadOnly);

  m_audio->setVolume(1.0);
  m_audio->start(m_device);
}

NotePreview::~NotePreview() { m_audio->suspend(); }
