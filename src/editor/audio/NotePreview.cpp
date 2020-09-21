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

static PxtoneUnitIODevice *device = nullptr;
static QAudioOutput *audio = nullptr;

NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int unit_no, int clock,
                         std::list<EVERECORD> additional_events, int duration,
                         std::shared_ptr<const pxtnWoice> starting_woice,
                         int bufferSize, QObject *parent)
    : QObject(parent),
      m_pxtn(pxtn),
      m_moo_params(moo_params) {
  // TODO: Initing a unit should not take 2 steps.
  std::shared_ptr<pxtnUnitTone> unit = std::make_shared<pxtnUnitTone>(starting_woice);
  moo_params->resetVoiceOn(unit.get());

  if (unit_no != -1)
    for (const EVERECORD *e = m_pxtn->evels->get_Records();
         e && e->clock <= clock; e = e->next) {
      if (e->unit_no == unit_no) {
        moo_params->processEvent(unit.get(), e, clock, -1, m_pxtn);
      }
    }

  for (const EVERECORD &e : additional_events)
    moo_params->processEvent(unit.get(), &e, clock, -1, pxtn);
  unit->Tone_KeyOn();

  // Because woices are shared ptrs we can be confident this won't break if the
  // woice is deleted.
  // We don't constantly reset because sometimes the audio engine forces
  // [life_count = 0] (say at the end of the sample)
  std::shared_ptr<const pxtnWoice> woice = unit->get_woice();
  for (int i = 0; i < woice->get_voice_num(); ++i) {
    // TODO: calculating the life count should be more automatic.
    auto tone = unit->get_tone(i);
    tone->on_count = duration;
    tone->life_count = duration + woice->get_instance(i)->env_release;
  }

  if (device == nullptr) {
    device = new PxtoneUnitIODevice(nullptr, m_pxtn, moo_params);
    device->open(QIODevice::ReadOnly);
  }

  device->units().push_back(unit);
  m_unit = device->units().end() - 1;

  if (audio == nullptr) {
    audio = new QAudioOutput(pxtoneAudioFormat(), device);
    audio->setVolume(1.0);
    audio->setBufferSize(bufferSize);
    audio->start(device);
  }
}

void NotePreview::processEvent(EVENTKIND kind, int32_t value) {
  m_moo_params->processNonOnEvent((*m_unit).get(), kind, value, m_pxtn);
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
                         int bufferSize, QObject *parent)
    : NotePreview(
          pxtn, moo_params, unit_no, clock,
          {ev(clock, EVENTKIND_KEY, pitch), ev(clock, EVENTKIND_VELOCITY, vel)},
          LONG_ON_VALUE, pxtn->Woice_Get(EVENTDEFAULT_VOICENO), bufferSize,
          parent) {}

NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int unit_no, int clock, int duration,
                         std::list<EVERECORD> additional_events, int bufferSize,
                         QObject *parent)
    : NotePreview(pxtn, moo_params, unit_no, clock, additional_events, duration,
                  pxtn->Woice_Get(EVENTDEFAULT_VOICENO), bufferSize, parent){};

NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int unit_no, int clock,
                         std::list<EVERECORD> additional_events, int bufferSize,
                         QObject *parent)
    : NotePreview(pxtn, moo_params, unit_no, clock, LONG_ON_VALUE,
                  additional_events, bufferSize, parent){};

NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int pitch, int vel, int duration,
                         std::shared_ptr<const pxtnWoice> woice, int bufferSize,
                         QObject *parent)
    : NotePreview(pxtn, moo_params, -1, 0,
                  {ev(0, EVENTKIND_KEY, pitch), ev(0, EVENTKIND_VELOCITY, vel)},
                  duration, woice, bufferSize, parent) {}

NotePreview::~NotePreview() {
  device->units().erase(m_unit);
}
