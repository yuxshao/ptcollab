#include "PxtoneIODevice.h"

#include <QDebug>

PxtoneIODevice::PxtoneIODevice(QObject *parent, const pxtnService *pxtn,
                               mooState *moo_state)
    : QIODevice(parent), pxtn(pxtn), moo_state(moo_state) {}

qint64 PxtoneIODevice::readData(char *data, qint64 maxlen) {
  int32_t filled_len = 0;
  if (!pxtn->Moo(*moo_state, data, int32_t(maxlen), &filled_len))
    emit MooError();
  return filled_len;
}
qint64 PxtoneIODevice::writeData(const char *data, qint64 len) {
  (void)data;
  return len;
}
