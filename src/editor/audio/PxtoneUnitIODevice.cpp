#include "PxtoneUnitIODevice.h"

#include <QDebug>

PxtoneUnitIODevice::PxtoneUnitIODevice(QObject *parent, const pxtnService *pxtn,
                                       const mooParams *moo_params)
    : QIODevice(parent), m_pxtn(pxtn), m_moo_params(moo_params) {}

qint64 PxtoneUnitIODevice::readData(char *data, qint64 maxlen) {
  memset(data, 0, maxlen);
  maxlen = maxlen * sizeof(char) / sizeof(int32_t);
  int filled = 0;

  int32_t time_pan_index = 0;
  while (filled < maxlen) {

    for (auto &unit : m_units) unit->Tone_Envelope();
    int result =
        m_pxtn->moo_tone_sample_multi(m_units, *m_moo_params, data + filled,
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
