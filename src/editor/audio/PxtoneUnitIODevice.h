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
  PxtoneUnitIODevice(QObject *parent, const pxtnService *pxtn,
                     const mooParams *moo_params, pxtnUnitTone *unit);
  virtual ~PxtoneUnitIODevice() { close(); };

 signals:
  void MooError();
  void ZeroLives();

 private:
  const pxtnService *m_pxtn;
  const mooParams *m_moo_params;
  pxtnUnitTone *m_unit;
  bool m_zero_lives;
  qint64 readData(char *data, qint64 maxlen);
  qint64 writeData(const char *data, qint64 len);
};

#endif  // PXTONEUNITIODEVICE_H
