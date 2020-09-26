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
  void setPlaying(bool playing);
  bool playing();

 signals:
  void MooError();
  void playingChanged(bool);

 private:
  const pxtnService *pxtn;
  mooState *moo_state;
  bool m_playing;
  qint64 readData(char *data, qint64 maxlen);
  qint64 writeData(const char *data, qint64 len);
};

#endif  // PXTONEIODEVICE_H
