#ifndef RTAUDIORENDERER_H
#define RTAUDIORENDERER_H

#include <RtAudio.h>

#include "pxtone/pxtnService.h"

class RtAudioRenderer {
  const pxtnService *m_pxtn;
  mooState m_moo_state;
  RtAudio m_dac;
  friend int rtAudioMoo(void *, void *, unsigned int, double,
                        RtAudioStreamStatus, void *);

 public:
  RtAudioRenderer(const pxtnService *pxtn);
  ~RtAudioRenderer();
};

#endif  // RTAUDIORENDERER_H
