#include "RtAudioRenderer.h"

#include <RtAudio.h>

#include <QDebug>

int MooTiming::now_no_wrap(const pxtnMaster *master) {
  return MasterExtended::unwrapClock(master, now_clock, num_loops);
}

const unsigned int BUFFER_FRAMES =
    256;  // At higher buffers, this does stutter. So you will need a
          // MooClock-like thing anyway.
int rtAudioMoo(void *outputBuffer, void * /*inputBuffer*/,
               unsigned int nBufferFrames, double /*streamTime*/,
               RtAudioStreamStatus status, void *userData) {
  RtAudioRenderer *renderer = (RtAudioRenderer *)userData;
  if (status) qWarning() << "Stream underflow detected!";
  int byte_per_smp;
  if (!renderer->m_pxtn->get_byte_per_smp(&byte_per_smp)) return 1;
  int size = nBufferFrames * byte_per_smp;

  if (renderer->m_playing.load()) {
    if (!renderer->m_pxtn->Moo(renderer->m_moo_state, outputBuffer, size,
                               nullptr, nullptr))
      return 1;
    renderer->m_moo_timing.store({renderer->m_moo_state.get_now_clock(),
                                  renderer->m_moo_state.num_loop});
  } else {
    memset(outputBuffer, 0, size);
  }

  return 0;
}

void RtAudioRenderer::setPlaying(bool playing) {
  bool not_playing = !playing;
  bool changed = m_playing.compare_exchange_strong(not_playing, playing);
  if (changed) emit playingChanged(playing);
}

void RtAudioRenderer::setBufferFrames(unsigned int numFrames) {
  stopStream();
  startStream(numFrames);
}

bool RtAudioRenderer::isPlaying() { return m_playing.load(); }

RtAudioRenderer::RtAudioRenderer(const pxtnService *pxtn,
                                 unsigned int numFrames)
    : m_pxtn(pxtn), m_dac(), m_playing(false) {
  pxtnVOMITPREPARATION prep{};
  prep.flags |= pxtnVOMITPREPFLAG_loop | pxtnVOMITPREPFLAG_unit_mute;
  prep.start_pos_sample = 0;
  prep.master_volume = 1;  // need to be able to dynamically change moo state
  if (!m_pxtn->moo_preparation(&prep, m_moo_state)) {
    throw std::runtime_error("moo prep failed");
  }
  startStream(numFrames);
}

void RtAudioRenderer::startStream(unsigned int numFrames) {
  if (m_dac.getDeviceCount() < 1)
    throw std::runtime_error("No audio devices found");

  RtAudio::StreamParameters parameters;
  parameters.deviceId = m_dac.getDefaultOutputDevice();
  parameters.nChannels = 2;
  parameters.firstChannel = 0;

  int sampleRate = 44100;
  m_dac.openStream(&parameters, NULL, RTAUDIO_SINT16, sampleRate, &numFrames,
                   &rtAudioMoo, this);
  m_dac.startStream();
}

void RtAudioRenderer::stopStream() {
  try {
    m_dac.stopStream();
  } catch (RtAudioError &e) {
    e.printMessage();
  }
  if (m_dac.isStreamOpen()) m_dac.closeStream();
}

MooTiming RtAudioRenderer::moo_timing() const { return m_moo_timing.load(); }

RtAudioRenderer::~RtAudioRenderer() { stopStream(); }
