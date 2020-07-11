#ifndef NOTEPREVIEW_H
#define NOTEPREVIEW_H
#include <QAudioOutput>
#include <QObject>

#include "PxtoneUnitIODevice.h"
#include "pxtone/pxtnService.h"
class NotePreview : public QObject {
  Q_OBJECT
 public:
  NotePreview(const pxtnService *pxtn, int unit_no, int clock, int pitch,
              int vel, QObject *parent = nullptr);
  NotePreview(const pxtnService *pxtn, int unit_no, int clock,
              QObject *parent = nullptr);
  NotePreview(const pxtnService *pxtn, int pitch, int vel, int duration,
              std::shared_ptr<const pxtnWoice> woice,
              QObject *parent = nullptr);
  void setVel(int vel) { m_unit.Tone_Velocity(vel); }
  void suspend() { m_audio->suspend(); }
  ~NotePreview();
 signals:
  void finished();

 private:
  NotePreview(const pxtnService *pxtn, int unit_no, int clock, int pitch,
              int vel, int duration,
              std::shared_ptr<const pxtnWoice> starting_woice,
              QObject *parent = nullptr);
  const pxtnService *m_pxtn;
  pxtnUnitTone m_unit;
  PxtoneUnitIODevice *m_device;
  QAudioOutput *m_audio;
};

#endif  // NOTEPREVIEW_H
