#include "pxtoneiodevice.h"
#include <QDebug>

PxtoneIODevice::PxtoneIODevice(pxtnService *pxtn) : QIODevice(), pxtn(pxtn)
{

}


qint64 PxtoneIODevice::readData(char *data, qint64 maxlen) {
    int32_t filled_len = 0;
    if (!pxtn->Moo(data, int32_t(maxlen), &filled_len)) qWarning() << "Moo error";
    return filled_len;
}
qint64 PxtoneIODevice::writeData(const char *data, qint64 len) {
    // do nothing
    return len;
}
