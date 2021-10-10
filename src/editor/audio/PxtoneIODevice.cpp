#include "PxtoneIODevice.h"

#include <QDebug>

PxtoneIODevice::PxtoneIODevice(QObject *parent, const pxtnService *pxtn,
                               mooState *moo_state)
    : QIODevice(parent), pxtn(pxtn), moo_state(moo_state), m_playing(false) {
  int32_t ch_num, sps;
  if (pxtn->get_destination_quality(&ch_num, &sps))
    m_volume_meters = std::vector<InterpolatedVolumeMeter>(
        ch_num, InterpolatedVolumeMeter(sps / 25, sps));
}

void PxtoneIODevice::setPlaying(bool playing) {
  bool changed = playing != m_playing;
  m_playing = playing;
  if (changed) emit playingChanged(playing);
}

bool PxtoneIODevice::playing() { return m_playing; }

const std::vector<InterpolatedVolumeMeter> &PxtoneIODevice::volumeLevels()
    const {
  return m_volume_meters;
}

qint64 PxtoneIODevice::readData(char *data, qint64 maxlen) {
  int32_t filled_len = 0;

  for (auto &m : m_volume_meters) m.new_batch();
  if (m_playing) {
    auto *meters = m_volume_meters.size() > 0 ? &m_volume_meters : nullptr;
    if (!pxtn->Moo(*moo_state, data, int32_t(maxlen), &filled_len, meters))
      emit MooError();
  } else {
    memset(data, 0, maxlen);

    int byte_per_smp;
    if (pxtn->get_byte_per_smp(&byte_per_smp)) {
      int smp_num = maxlen / byte_per_smp / m_volume_meters.size();
      for (auto &m : m_volume_meters)
        for (int i = 0; i < smp_num; ++i) m.insert(0);
    }
    filled_len = maxlen;
  }
  return filled_len;
}
qint64 PxtoneIODevice::writeData(const char *data, qint64 len) {
  (void)data;
  return len;
}
