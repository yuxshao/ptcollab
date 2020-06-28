#ifndef PXTONEUNITIODEVICE_H
#define PXTONEUNITIODEVICE_H
#include <QIODevice>

#include "pxtone/pxtnService.h"
#include "pxtone/pxtnWoice.h"

/* An IO device for playing audio from a custom unit separately from the moo
 * stream. */
class PxtoneUnitIODevice : public QIODevice {
  Q_OBJECT

 public:
  PxtoneUnitIODevice(QObject *parent, const pxtnService *pxtn, int unit_no,
                     int pitch, int clock, int vel);
  virtual ~PxtoneUnitIODevice() { close(); };

  void stopTone();

 signals:
  void MooError();

 private:
  const pxtnService *m_pxtn;
  int m_unit_no, m_pitch, m_clock, m_vel;
  pxtnUnit m_unit;
  bool on;
  qint64 readData(char *data, qint64 maxlen);
  qint64 writeData(const char *data, qint64 len);
};

#endif  // PXTONEUNITIODEVICE_H
