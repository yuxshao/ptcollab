#ifndef PXTONEUNITIODEVICE_H
#define PXTONEUNITIODEVICE_H
#include <QIODevice>

#include "pxtone/pxtnService.h"
#include "pxtone/pxtnWoice.h"

/* An IO device for playing audio from a single voice separately from the moo
 * stream. */
class PxtoneUnitIODevice : public QIODevice {
  Q_OBJECT

 public:
  PxtoneUnitIODevice(QObject *parent, pxtnService *pxtn, int unit_no,
                     int pitch);

  virtual ~PxtoneUnitIODevice() { close(); };

  void stopTone();

 signals:
  void MooError();

 private:
  pxtnService *pxtn;
  pxtnVOICETONE voice_tones[pxtnMAX_UNITCONTROLVOICE];

  int unit_no;
  int pitch;
  bool on;
  qint64 readData(char *data, qint64 maxlen);
  qint64 writeData(const char *data, qint64 len);
};

#endif  // PXTONEUNITIODEVICE_H
