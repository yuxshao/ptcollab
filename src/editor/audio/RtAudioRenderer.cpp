#include "RtAudioRenderer.h"

#include <RtAudio.h>

#include <QDebug>

int MooTiming::now_no_wrap(const pxtnMaster *master) {
  return MasterExtended::unwrapClock(master, now_clock, num_loops);
}

const unsigned int BUFFER_FRAMES =
    256;  // At higher buffers, this does stuttter. So you will need a
          // MooClock-like thing anyway.
int rtAudioMoo(void *outputBuffer, void * /*inputBuffer*/,
               unsigned int nBufferFrames, double /*streamTime*/,
               RtAudioStreamStatus status, void *userData) {
  RtAudioRenderer *renderer = (RtAudioRenderer *)userData;
  if (status) qWarning() << "Stream underflow detected!";

  int byte_per_smp;
  if (!renderer->m_pxtn->get_byte_per_smp(&byte_per_smp)) return 1;

  int size = nBufferFrames * byte_per_smp;
  if (!renderer->m_pxtn->Moo(renderer->m_moo_state, outputBuffer, size, nullptr,
                             nullptr))
    return 1;
  renderer->m_moo_timing.store(
      {renderer->m_moo_state.get_now_clock(), renderer->m_moo_state.num_loop});

  return 0;
}

RtAudioRenderer::RtAudioRenderer(const pxtnService *pxtn)
    : m_pxtn(pxtn), m_dac() {
  if (m_dac.getDeviceCount() < 1)
    throw std::runtime_error("No audio devices found");

  qDebug() << "lock free" << m_moo_timing.is_lock_free();
  RtAudio::StreamParameters parameters;
  parameters.deviceId = m_dac.getDefaultOutputDevice();
  parameters.nChannels = 2;
  parameters.firstChannel = 0;

  pxtnVOMITPREPARATION prep{};
  prep.flags |= pxtnVOMITPREPFLAG_loop | pxtnVOMITPREPFLAG_unit_mute;
  prep.start_pos_sample = 0;
  prep.master_volume = 1;  // need to be able to dynamically change moo state
  if (!m_pxtn->moo_preparation(&prep, m_moo_state)) {
    throw std::runtime_error("moo prep failed");
  }

  int sampleRate = 44100;
  unsigned int bufferFrames = BUFFER_FRAMES;
  m_dac.openStream(&parameters, NULL, RTAUDIO_SINT16, sampleRate, &bufferFrames,
                   &rtAudioMoo, this);
  m_dac.startStream();
  qDebug() << "started stream" << bufferFrames;
}

MooTiming RtAudioRenderer::moo_timing() const { return m_moo_timing.load(); }

RtAudioRenderer::~RtAudioRenderer() {
  try {
    m_dac.stopStream();
  } catch (RtAudioError &e) {
    e.printMessage();
  }
  if (m_dac.isStreamOpen()) m_dac.closeStream();
}
