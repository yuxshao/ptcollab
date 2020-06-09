#include "PxtoneUnitIODevice.h"

#include <QDebug>

constexpr int32_t LONG_ON_VALUE = 100000000;
// TODO: make sure not to leak by closing at end
PxtoneUnitIODevice::PxtoneUnitIODevice(QObject *parent, pxtnService const *pxtn,
                                       int unit_no, int pitch)
    : QIODevice(parent), pxtn(pxtn), unit_no(unit_no), pitch(pitch) {
  if (!pxtn->moo_get_pxtnVOICETONE(pxtn->Unit_Get(unit_no), voice_tones))
    qWarning() << "Could not initialize voice tone.";

  // We don't constantly reset because sometimes the audio engine forces
  // [life_count = 0] (say at the end of the sample)
  for (int i = 0; i < pxtnMAX_UNITCONTROLVOICE; ++i)
    voice_tones[i].on_count = voice_tones[i].life_count = LONG_ON_VALUE;
}

void PxtoneUnitIODevice::stopTone() {
  for (int i = 0; i < pxtnMAX_UNITCONTROLVOICE; ++i)
    voice_tones[i].on_count = voice_tones[i].life_count = 0;
}

qint64 PxtoneUnitIODevice::readData(char *data, qint64 maxlen) {
  maxlen = maxlen * sizeof(char) / sizeof(int32_t);
  const pxtnUnit *p_u = pxtn->Unit_Get(unit_no);
  int filled = 0;
  while (filled < maxlen) {
    p_u->Tone_Envelope_Custom(voice_tones);
    int result = pxtn->moo_tone_sample_custom(p_u, voice_tones, data + filled,
                                              maxlen - filled, pitch);
    filled += result;
    if (result == 0) break;
  }
  return filled;
}
qint64 PxtoneUnitIODevice::writeData(const char *data, qint64 len) {
  (void)data;
  return len;
}
