#ifndef RTAUDIORENDERER_H
#define RTAUDIORENDERER_H

#include <RtAudio.h>

#include <QObject>
#include <atomic>

#include "pxtone/pxtnService.h"

struct MooTiming {
  int now_clock;
  int num_loops;

  int now_no_wrap(const pxtnMaster *master);
};

class RtAudioRenderer : public QObject {
  Q_OBJECT

  const pxtnService *m_pxtn;
  std::atomic<MooTiming> m_moo_timing;
  mooState m_moo_state;
  RtAudio m_dac;
  std::atomic<bool> m_playing;
  friend int rtAudioMoo(void *, void *, unsigned int, double,
                        RtAudioStreamStatus, void *);
 signals:
  void playingChanged(bool);

 public:
  RtAudioRenderer(const pxtnService *pxtn, unsigned int numFrames);
  MooTiming moo_timing() const;

  ~RtAudioRenderer();
  void setPlaying(bool playing);
  void setBufferFrames(unsigned int);
  bool isPlaying();

 private:
  void startStream(unsigned int numFrames);
  void stopStream();
};

#endif  // RTAUDIORENDERER_H
