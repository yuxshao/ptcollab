#include "PxtoneIODevice.h"

#include <QDebug>

PxtoneIODevice::PxtoneIODevice(QObject *parent, const pxtnService *pxtn,
                               mooState *moo_state)
    : QIODevice(parent), pxtn(pxtn), moo_state(moo_state), m_playing(false) {
  int32_t ch_num, sps;
  if (pxtn->get_destination_quality(&ch_num, &sps))
    m_volume_meters = std::unique_ptr<std::vector<VolumeMeter>>(
        new std::vector<VolumeMeter>(ch_num, VolumeMeter(sps / 100, sps)));
}

void PxtoneIODevice::setPlaying(bool playing) {
  bool changed = playing != m_playing;
  m_playing = playing;
  if (changed) emit playingChanged(playing);
}

bool PxtoneIODevice::playing() { return m_playing; }

qint64 PxtoneIODevice::readData(char *data, qint64 maxlen) {
  int32_t filled_len = 0;
  if (m_playing) {
    if (!pxtn->Moo(*moo_state, data, int32_t(maxlen), &filled_len,
                   m_volume_meters.get()))
      emit MooError();
  } else {
    memset(data, 0, maxlen);

    int byte_per_smp;
    if (m_volume_meters && pxtn->get_byte_per_smp(&byte_per_smp)) {
      int smp_num = maxlen / byte_per_smp / m_volume_meters->size();
      for (auto &m : *m_volume_meters)
        for (int i = 0; i < smp_num; ++i) m.insert(0);
    }
    filled_len = maxlen;
  }
  if (m_volume_meters) emit volumeLevelChanged(*m_volume_meters);
  return filled_len;
}
qint64 PxtoneIODevice::writeData(const char *data, qint64 len) {
  (void)data;
  return len;
}
