#include "AudioFormat.h"
#include "editor/Settings.h"

static QAudioFormat make() {
  QAudioFormat format;
  int channel_num = 2;
  int sample_rate = Settings::SampleRate::get();
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
