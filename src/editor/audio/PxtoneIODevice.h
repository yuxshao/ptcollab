#ifndef PXTONEIODEVICE_H
#define PXTONEIODEVICE_H

#include <QIODevice>

#include "VolumeMeter.h"
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
  double current_volume_dbfs();
  const std::vector<InterpolatedVolumeMeter> &volumeLevels() const;

 signals:
  void MooError();
  void playingChanged(bool);

 private:
  const pxtnService *pxtn;
  mooState *moo_state;
  bool m_playing;
  std::vector<InterpolatedVolumeMeter> m_volume_meters;
  int32_t m_bytes_per_sec;

  qint64 readData(char *data, qint64 maxlen) override;
  qint64 writeData(const char *data, qint64 len) override;
  qint64 bytesAvailable() const override;
};

#endif  // PXTONEIODEVICE_H
