#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QIODevice>

#include "miniaudio.h"

class AudioOutput {
  ma_waveform sineWave;
  ma_device_config deviceConfig;
  ma_device device;
  ma_waveform_config sineWaveConfig;
  std::map<int, QIODevice *> m_devices;
  int m_next_id;
  AudioOutput();
  friend void data_callback(ma_device *pDevice, void *pOutput,
                            const void *pInput, ma_uint32 frameCount);

 public:
  int addDevice(QIODevice *dev);
  void removeDevice(int);

  static AudioOutput &output();
};

#endif  // AUDIOOUTPUT_H
