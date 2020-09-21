#include "PxtoneUnitIODevice.h"

#include <QDebug>

PxtoneUnitIODevice::PxtoneUnitIODevice(QObject *parent, const pxtnService *pxtn,
                                       const mooParams *moo_params,
                                       std::shared_ptr<pxtnUnitTone> unit)
    : QIODevice(parent), m_pxtn(pxtn), m_unit(nullptr) {
  set(moo_params, unit);
}

void PxtoneUnitIODevice::set(const mooParams *moo_params,
                             std::shared_ptr<pxtnUnitTone> unit) {
  m_moo_params = moo_params;
  m_unit = unit;
}

void PxtoneUnitIODevice::stopNote() { m_unit->Tone_ZeroLives(); }

qint64 PxtoneUnitIODevice::readData(char *data, qint64 maxlen) {
  memset(data, 0, maxlen);
  maxlen = maxlen * sizeof(char) / sizeof(int32_t);
  int filled = 0;

  int32_t time_pan_index = 0;
  while (filled < maxlen) {
    m_unit->Tone_Envelope();
    int result =
        m_pxtn->moo_tone_sample(m_unit.get(), *m_moo_params, data + filled,
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
