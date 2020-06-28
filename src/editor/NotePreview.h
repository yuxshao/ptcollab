#ifndef NOTEPREVIEW_H
#define NOTEPREVIEW_H
#include <QAudioOutput>
#include <QObject>

#include "PxtoneUnitIODevice.h"
#include "pxtone/pxtnService.h"
class NotePreview : public QObject {
  Q_OBJECT
 public:
  NotePreview(const pxtnService *pxtn, int unit_no, int pitch, int clock,
              int vel, QObject *parent = nullptr);
  void setVel(int vel) { m_unit.Tone_Velocity(vel); }
  void suspend() { m_audio->suspend(); }
  ~NotePreview();

 private:
  const pxtnService *m_pxtn;
  pxtnUnit m_unit;
  PxtoneUnitIODevice *m_device;
  QAudioOutput *m_audio;
};

#endif  // NOTEPREVIEW_H
