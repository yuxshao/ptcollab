#include "AudioOutput.h"

#include <QDebug>

#include "miniaudio.h"

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                   ma_uint32 frameCount) {
  AudioOutput *o = (AudioOutput *)pDevice->pUserData;
  ma_waveform *pSineWave = &o->sineWave;
  ma_waveform_read_pcm_frames(pSineWave, pOutput, frameCount);

  (void)pInput;
}

AudioOutput::AudioOutput() : m_next_id(0) {
  sineWaveConfig = ma_waveform_config_init(ma_format_f32, 2, 48000,
                                           ma_waveform_type_sine, 0.2, 220);
  ma_waveform_init(&sineWaveConfig, &sineWave);

  deviceConfig = ma_device_config_init(ma_device_type_playback);
  deviceConfig.playback.format = ma_format_f32;
  deviceConfig.playback.channels = 2;
  deviceConfig.sampleRate = 44100;
  deviceConfig.dataCallback = data_callback;
  deviceConfig.pUserData = this;

  if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
    printf("Failed to open playback device.\n");
  }

  printf("Device Name: %s\n", device.playback.name);

  if (ma_device_start(&device) != MA_SUCCESS) {
    printf("Failed to start playback device.\n");
    ma_device_uninit(&device);
  }
}

int AudioOutput::addDevice(QIODevice *dev) {
  m_devices[m_next_id++] = dev;
  return m_next_id - 1;
}
void AudioOutput::removeDevice(int id) { m_devices.erase(id); }

AudioOutput &AudioOutput::output() {
  static AudioOutput o;
  return o;
}
