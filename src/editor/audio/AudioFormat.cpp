#include "AudioFormat.h"

static QAudioFormat make() {
  QAudioFormat format;
  int channel_num = 2;
  int sample_rate = 44100;
  format.setSampleRate(sample_rate);
  format.setChannelCount(channel_num);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  format.setSampleSize(16);  // Fixed in pxtone
  format.setCodec("audio/pcm");
  format.setByteOrder(QAudioFormat::LittleEndian);
  format.setSampleType(QAudioFormat::SignedInt);
#else
  format.setSampleFormat(QAudioFormat::Int16);  // Fixed in pxtone
  // format.setByteOrder(QAudioFormat::LittleEndian);
#endif
  return format;
}

QAudioFormat pxtoneAudioFormat() {
  static QAudioFormat format = make();
  return format;
}
