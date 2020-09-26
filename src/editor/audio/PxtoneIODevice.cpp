#include "PxtoneIODevice.h"

#include <QDebug>

PxtoneIODevice::PxtoneIODevice(QObject *parent, const pxtnService *pxtn,
                               mooState *moo_state)
    : QIODevice(parent), pxtn(pxtn), moo_state(moo_state), m_playing(false) {}

void PxtoneIODevice::setPlaying(bool playing) {
  bool changed = playing != m_playing;
  m_playing = playing;
  if (changed) emit playingChanged(playing);
}

bool PxtoneIODevice::playing() { return m_playing; }

qint64 PxtoneIODevice::readData(char *data, qint64 maxlen) {
  if (m_playing) {
    int32_t filled_len = 0;
    if (!pxtn->Moo(*moo_state, data, int32_t(maxlen), &filled_len))
      emit MooError();
    return filled_len;
  } else {
    memset(data, 0, maxlen);
    return maxlen;
  }
}
qint64 PxtoneIODevice::writeData(const char *data, qint64 len) {
  (void)data;
  return len;
}
