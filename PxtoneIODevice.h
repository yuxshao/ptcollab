#ifndef PXTONEIODEVICE_H
#define PXTONEIODEVICE_H

#include <QIODevice>
#include "pxtone/pxtnService.h"

/**
 * @brief A pxtnService wrapper for QTAudioOutput.
 */
class PxtoneIODevice : public QIODevice
{
public:
    PxtoneIODevice(pxtnService *pxtn);
private:
    pxtnService *pxtn;
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);
};

#endif // PXTONEIODEVICE_H
