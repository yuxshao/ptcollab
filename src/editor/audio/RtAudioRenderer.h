#ifndef RTAUDIORENDERER_H
#define RTAUDIORENDERER_H

#include <RtAudio.h>

#include <atomic>

#include "pxtone/pxtnService.h"

struct MooTiming {
  int now_clock;
  int num_loops;

  int now_no_wrap(const pxtnMaster *master);
};

class RtAudioRenderer {
  const pxtnService *m_pxtn;
  std::atomic<MooTiming> m_moo_timing;
  mooState m_moo_state;
  RtAudio m_dac;
  friend int rtAudioMoo(void *, void *, unsigned int, double,
                        RtAudioStreamStatus, void *);

 public:
  RtAudioRenderer(const pxtnService *pxtn);
  MooTiming moo_timing() const;

  ~RtAudioRenderer();
};

#endif  // RTAUDIORENDERER_H
