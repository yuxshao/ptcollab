#ifndef PXTONEUNITIODEVICE_H
#define PXTONEUNITIODEVICE_H
#include <QIODevice>
#include <map>

#include "pxtone/pxtnService.h"
#include "pxtone/pxtnWoice.h"

/* An IO device for playing audio from a number of custom units separately from
 * the moo stream. It's not just a single unit because multiplexing and having
 * one audio output continuously playing is significantly faster on Windows.
 */
class PxtoneUnitIODevice : public QIODevice {
  Q_OBJECT

 public:
  PxtoneUnitIODevice(QObject *parent, const pxtnService *pxtn,
                     const mooParams *moo_params);
  void removeUnit(int unit_id);
  int addUnit(pxtnUnitTone *unit);
  virtual ~PxtoneUnitIODevice() { close(); };

 signals:
  void MooError();

 private:
  std::map<int, pxtnUnitTone *> m_units;
  const pxtnService *m_pxtn;
  const mooParams *m_moo_params;
  int m_next_unit_id;
  qint64 readData(char *data, qint64 maxlen);
  qint64 writeData(const char *data, qint64 len);
};

#endif  // PXTONEUNITIODEVICE_H
