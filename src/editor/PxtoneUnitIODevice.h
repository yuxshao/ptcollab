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
  PxtoneUnitIODevice(QObject *parent, const pxtnService *pxtn, pxtnUnit *unit);
  virtual ~PxtoneUnitIODevice() { close(); };

 signals:
  void MooError();

 private:
  const pxtnService *m_pxtn;
  pxtnUnit *m_unit;
  qint64 readData(char *data, qint64 maxlen);
  qint64 writeData(const char *data, qint64 len);
};

#endif  // PXTONEUNITIODEVICE_H
