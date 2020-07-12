#ifndef PXTONEIODEVICE_H
#define PXTONEIODEVICE_H

#include <QIODevice>

#include "pxtone/pxtnService.h"

/**
 * @brief A pxtnService wrapper for QTAudioOutput.
 */
class PxtoneIODevice : public QIODevice {
  Q_OBJECT
 public:
  PxtoneIODevice(QObject *parent, const pxtnService *pxtn, mooState *moo_state);
  virtual ~PxtoneIODevice(){};

 signals:
  void MooError();

 private:
  const pxtnService *pxtn;
  mooState *moo_state;
  qint64 readData(char *data, qint64 maxlen);
  qint64 writeData(const char *data, qint64 len);
};

#endif  // PXTONEIODEVICE_H
