#include "PxtoneUnitIODevice.h"

#include <QDebug>

PxtoneUnitIODevice::PxtoneUnitIODevice(QObject *parent, const pxtnService *pxtn,
                                       pxtnUnitTone *unit)
    : QIODevice(parent), m_pxtn(pxtn), m_unit(unit), m_zero_lives(false) {}

qint64 PxtoneUnitIODevice::readData(char *data, qint64 maxlen) {
  memset(data, 0, maxlen);
  maxlen = maxlen * sizeof(char) / sizeof(int32_t);
  int filled = 0;

  int32_t time_pan_index = 0;
  while (filled < maxlen) {
    m_unit->Tone_Envelope();
    int result = m_pxtn->moo_tone_sample(m_unit, data + filled, maxlen - filled,
                                         time_pan_index);
    time_pan_index = (time_pan_index + 1) & (pxtnBUFSIZE_TIMEPAN - 1);
    filled += result;
    if (result == 0) break;
  }

  bool zero_lives = true;
  for (int i = 0; i < m_unit->get_woice()->get_voice_num(); ++i)
    if (m_unit->get_tone(i)->life_count != 0) zero_lives = false;
  if (zero_lives && !m_zero_lives) emit ZeroLives();
  m_zero_lives = zero_lives;

  return filled;
}
qint64 PxtoneUnitIODevice::writeData(const char *data, qint64 len) {
  (void)data;
  return len;
}
