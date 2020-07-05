#include "NotePreview.h"

#include <QAudioFormat>
#include <QAudioOutput>

#include "AudioFormat.h"
#include "NotePreview.h"
#include "PxtoneUnitIODevice.h"

constexpr int32_t LONG_ON_VALUE = 100000000;
NotePreview::NotePreview(const pxtnService *pxtn, int unit_no, int clock,
                         int pitch, int vel, int duration,
                         std::shared_ptr<const pxtnWoice> starting_woice,
                         QObject *parent)
    : QObject(parent), m_pxtn(pxtn) {
  int32_t dst_sps, dst_ch_num;
  m_pxtn->get_destination_quality(&dst_sps, &dst_ch_num);

  // TODO: Initing a unit should not take 2 steps.
  m_unit.Tone_Init(starting_woice);
  m_pxtn->moo_params()->resetVoiceOn(&m_unit);

  if (unit_no != -1)
    for (const EVERECORD *e = m_pxtn->evels->get_Records();
         e && e->clock <= clock; e = e->next) {
      if (e->unit_no == unit_no) {
        m_pxtn->moo_params()->processEvent(&m_unit, unit_no, e, clock, dst_sps,
                                           dst_ch_num, -1, m_pxtn);
      }
    }

  // Because woices are shared ptrs we can be confident this won't break if the
  // woice is deleted.
  if (pitch != -1) m_unit.Tone_Key(pitch);
  if (vel != -1) m_unit.Tone_Velocity(vel);
  m_unit.Tone_KeyOn();

  // We don't constantly reset because sometimes the audio engine forces
  // [life_count = 0] (say at the end of the sample)
  std::shared_ptr<const pxtnWoice> woice = m_unit.get_woice();
  for (int i = 0; i < woice->get_voice_num(); ++i) {
    // TODO: calculating the life count should be more automatic.
    auto tone = m_unit.get_tone(i);
    tone->on_count = duration;
    tone->life_count = duration + woice->get_instance(i)->env_release;
  }

  m_device = new PxtoneUnitIODevice(this, m_pxtn, &m_unit);
  m_device->open(QIODevice::ReadOnly);

  m_audio = new QAudioOutput(pxtoneAudioFormat(), m_device);
  m_audio->setVolume(1.0);
  m_audio->start(m_device);
  connect(m_device, &PxtoneUnitIODevice::ZeroLives, this,
          &NotePreview::finished);
}
NotePreview::NotePreview(const pxtnService *pxtn, int unit_no, int clock,
                         int pitch, int vel, QObject *parent)
    : NotePreview(pxtn, unit_no, clock, pitch, vel, LONG_ON_VALUE,
                  pxtn->Woice_Get(EVENTDEFAULT_VOICENO), parent) {}

NotePreview::NotePreview(const pxtnService *pxtn, int unit_no, int clock,
                         QObject *parent)
    : NotePreview(pxtn, unit_no, clock, -1, -1, parent){};

NotePreview::NotePreview(const pxtnService *pxtn, int pitch, int vel,
                         int duration, std::shared_ptr<const pxtnWoice> woice,
                         QObject *parent)
    : NotePreview(pxtn, -1, -1, pitch, vel, duration, woice, parent) {}

NotePreview::~NotePreview() {
  // seems to fix a crash that'd happen if I change quickly between woices
  // causing note previews.
  suspend();
}
