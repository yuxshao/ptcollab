#include "NotePreview.h"

#include <QAudioFormat>
#include <QAudioOutput>
#include <QDebug>

#include "AudioFormat.h"
#include "NotePreview.h"
#include "PxtoneUnitIODevice.h"
// TODO: m_unit should probably be not a member variable but an r-value passed
// to the child.
constexpr int32_t LONG_ON_VALUE = 100000000;
NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int unit_no, int clock,
                         std::list<EVERECORD> additional_events, int duration,
                         std::shared_ptr<const pxtnWoice> starting_woice,
                         QObject *parent)
    : QObject(parent),
      m_pxtn(pxtn),
      m_unit(starting_woice),
      m_moo_params(moo_params) {
  // TODO: Initing a unit should not take 2 steps.
  moo_params->resetVoiceOn(&m_unit);

  if (unit_no != -1)
    for (const EVERECORD *e = m_pxtn->evels->get_Records();
         e && e->clock <= clock; e = e->next) {
      if (e->unit_no == unit_no) {
        moo_params->processEvent(&m_unit, unit_no, e, clock, -1, m_pxtn);
      }
    }

  for (const EVERECORD &e : additional_events)
    moo_params->processEvent(&m_unit, unit_no, &e, clock, -1, pxtn);
  m_unit.Tone_KeyOn();

  // Because woices are shared ptrs we can be confident this won't break if the
  // woice is deleted.
  // We don't constantly reset because sometimes the audio engine forces
  // [life_count = 0] (say at the end of the sample)
  std::shared_ptr<const pxtnWoice> woice = m_unit.get_woice();
  for (int i = 0; i < woice->get_voice_num(); ++i) {
    // TODO: calculating the life count should be more automatic.
    auto tone = m_unit.get_tone(i);
    tone->on_count = duration;
    tone->life_count = duration + woice->get_instance(i)->env_release;
  }

  m_device = new PxtoneUnitIODevice(this, m_pxtn, moo_params, &m_unit);
  m_device->open(QIODevice::ReadOnly);

  m_audio = new QAudioOutput(pxtoneAudioFormat(), m_device);
  m_audio->setVolume(1.0);
  m_audio->start(m_device);
  connect(m_device, &PxtoneUnitIODevice::ZeroLives, this,
          &NotePreview::finished);
}

void NotePreview::processEvent(EVENTKIND kind, int32_t value) {
  m_moo_params->processNonOnEvent(&m_unit, kind, value, m_pxtn);
}

static EVERECORD ev(int32_t clock, EVENTKIND kind, int32_t value) {
  EVERECORD e;
  e.clock = clock;
  e.kind = kind;
  e.value = value;
  return e;
}

NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int unit_no, int clock, int pitch, int vel,
                         QObject *parent)
    : NotePreview(
          pxtn, moo_params, unit_no, clock,
          {ev(clock, EVENTKIND_KEY, pitch), ev(clock, EVENTKIND_VELOCITY, vel)},
          LONG_ON_VALUE, pxtn->Woice_Get(EVENTDEFAULT_VOICENO), parent) {}

NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int unit_no, int clock,
                         std::list<EVERECORD> additional_events,
                         QObject *parent)
    : NotePreview(pxtn, moo_params, unit_no, clock, additional_events,
                  LONG_ON_VALUE, pxtn->Woice_Get(EVENTDEFAULT_VOICENO),
                  parent){};

NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int pitch, int vel, int duration,
                         std::shared_ptr<const pxtnWoice> woice,
                         QObject *parent)
    : NotePreview(pxtn, moo_params, -1, 0,
                  {ev(0, EVENTKIND_KEY, pitch), ev(0, EVENTKIND_VELOCITY, vel)},
                  duration, woice, parent) {}

NotePreview::~NotePreview() {
  // seems to fix a crash that'd happen if I change quickly between woices
  // causing note previews.
  suspend();
}
