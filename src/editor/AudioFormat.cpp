#include "AudioFormat.h"

static QAudioFormat make() {
  QAudioFormat format;
  int channel_num = 2;
  int sample_rate = 44100;
  format.setSampleRate(sample_rate);
  format.setChannelCount(channel_num);
  format.setSampleSize(16);  // Fixed in pxtone
  format.setCodec("audio/pcm");
  format.setByteOrder(QAudioFormat::LittleEndian);
  format.setSampleType(QAudioFormat::SignedInt);
  return format;
}

QAudioFormat pxtoneAudioFormat() {
  static QAudioFormat format = make();
  return format;
}
