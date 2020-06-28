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
  void suspend() { m_audio->suspend(); }
  ~NotePreview();

 private:
  PxtoneUnitIODevice *m_device;
  QAudioOutput *m_audio;
};

#endif  // NOTEPREVIEW_H
