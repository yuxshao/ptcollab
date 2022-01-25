#ifndef NOTEPREVIEW_H
#define NOTEPREVIEW_H
#include <QAudioOutput>
#include <QObject>

#include "PxtoneUnitIODevice.h"
#include "pxtone/pxtnService.h"
class NotePreview : public QObject {
  Q_OBJECT
 public:
  // To play a unit at a pitch and velocity. E.g., note input.
  NotePreview(const pxtnService *pxtn, const mooParams *moo_params, int unit_no,
              int clock, int pitch, int vel, int bufferSize, bool chordPreview,
              QObject *parent);

  // To play a unit with some arbitrary additional events. E.g., param input.
  NotePreview(const pxtnService *pxtn, const mooParams *moo_params, int unit_no,
              int clock, std::list<EVERECORD> additional_events, int bufferSize,
              bool chordPreview, QObject *parent);

  // Seems like the same as above but with a duration.
  NotePreview(const pxtnService *pxtn, const mooParams *moo_params, int unit_no,
              int clock, int duration, std::list<EVERECORD> additional_events,
              int bufferSize, bool chordPreview, QObject *parent);

  // Play a specific woice that you provide. E.g., selecting a candidate woice.
  NotePreview(const pxtnService *pxtn, const mooParams *moo_params, int pitch,
              int vel, int duration, std::shared_ptr<const pxtnWoice> woice,
              int bufferSize, QObject *parent);
  void processEvent(EVENTKIND kind, int32_t value);
  void resetOn(int duration);
  ~NotePreview();

 private:
  NotePreview(const pxtnService *pxtn, const mooParams *moo_params, int unit_no,
              int clock, std::list<EVERECORD> additional_events, int duration,
              std::shared_ptr<const pxtnWoice> starting_woice, int bufferSize,
              bool chordPreview, QObject *parent = nullptr);
  const pxtnService *m_pxtn;
  // PxtoneUnitIODevice *m_device;
  std::vector<int> m_unit_ids;
  pxtnUnitTone *m_this_unit;
  std::unique_ptr<pxtnUnitTone> m_unit;
  std::unique_ptr<mooState> m_moo_state;
  const mooParams *m_moo_params;
  // QAudioOutput *m_audio;
};

#endif  // NOTEPREVIEW_H
