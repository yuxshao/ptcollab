#include "AudioFormat.h"

static QAudioFormat make() {
  QAudioFormat format;
  int channel_num = 2;
  int sample_rate = 44100;
  format.setSampleRate(sample_rate);
  format.setChannelCount(channel_num);
  format.setSampleFormat(QAudioFormat::Int16);  // Fixed in pxtone
  // format.setByteOrder(QAudioFormat::LittleEndian);
  return format;
}

QAudioFormat pxtoneAudioFormat() {
  static QAudioFormat format = make();
  return format;
}
