#include "PxtoneUnitIODevice.h"

#include <QDebug>

constexpr int32_t LONG_ON_VALUE = 100000000;
// TODO: make sure not to leak by closing at end
// TODO: make sure unit is owned by pxtoneIODevice, not the notepreview.
PxtoneUnitIODevice::PxtoneUnitIODevice(QObject *parent, const pxtnService *pxtn,
                                       int unit_no, int pitch, int clock,
                                       int vel)
    : QIODevice(parent), m_pxtn(pxtn), m_unit() {
  int32_t dst_sps, dst_ch_num;
  m_pxtn->get_destination_quality(&dst_sps, &dst_ch_num);

  // TODO: Yeah, initing a unit for sampling is pretty bad right now.
  m_unit.Tone_Init();
  m_pxtn->moo_params()->resetVoiceOn(&m_unit, EVENTDEFAULT_VOICENO, m_pxtn);
  for (const EVERECORD *e = m_pxtn->evels->get_Records(); e->clock <= clock;
       ++e) {
    if (e->unit_no == unit_no) {
      m_pxtn->moo_params()->processEvent(&m_unit, unit_no, e, clock, dst_sps,
                                         dst_ch_num, m_pxtn);
    }
  }

  m_unit.Tone_Key(pitch);
  m_unit.Tone_Velocity(vel);
  m_unit.Tone_KeyOn();

  // We don't constantly reset because sometimes the audio engine forces
  // [life_count = 0] (say at the end of the sample)
  for (int i = 0; i < pxtnMAX_UNITCONTROLVOICE; ++i) {
    auto tone = m_unit.get_tone(i);
    tone->on_count = tone->life_count = LONG_ON_VALUE;
  }
}

void PxtoneUnitIODevice::stopTone() { m_unit.Tone_ZeroLives(); }

qint64 PxtoneUnitIODevice::readData(char *data, qint64 maxlen) {
  memset(data, 0, maxlen);
  maxlen = maxlen * sizeof(char) / sizeof(int32_t);
  int filled = 0;

  int32_t time_pan_index = 0;
  while (filled < maxlen) {
    m_unit.Tone_Envelope();
    int result = m_pxtn->moo_tone_sample(&m_unit, data + filled,
                                         maxlen - filled, time_pan_index);
    time_pan_index = (time_pan_index + 1) & (pxtnBUFSIZE_TIMEPAN - 1);
    filled += result;
    if (result == 0) break;
  }
  return filled;
}
qint64 PxtoneUnitIODevice::writeData(const char *data, qint64 len) {
  (void)data;
  return len;
}
