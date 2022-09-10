#include "NotePreview.h"

#include <QAudioFormat>
#include <QAudioOutput>
#include <QDebug>
#include <QSettings>

#include "AudioFormat.h"
#include "NotePreview.h"
#include "PxtoneUnitIODevice.h"
#include "editor/Settings.h"

// TODO: m_unit should probably be not a member variable but an r-value passed
// to the child.
constexpr int32_t LONG_ON_VALUE = 100000000;

static PxtoneUnitIODevice *device = nullptr;
static QAudioOutput *audio = nullptr;

NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int unit_no, int clock,
                         std::list<EVERECORD> additional_events, int duration,
                         std::shared_ptr<const pxtnWoice> starting_woice,
                         int bufferSize, bool chord_preview, QObject *parent)
    : QObject(parent),
      m_pxtn(pxtn),
      m_unit(nullptr),
      m_moo_state(nullptr),
      m_moo_params(moo_params) {
  clock = MasterExtended::wrapClock(m_pxtn->master, clock);
  if (!chord_preview || unit_no == -1) {
    m_unit = std::make_unique<pxtnUnitTone>(starting_woice);
    m_this_unit = m_unit.get();
    moo_params->resetVoiceOn(m_this_unit);
    if (unit_no != -1)
      for (const EVERECORD *e = m_pxtn->evels->get_Records();
           e && e->clock <= clock; e = e->next) {
        if (e->unit_no == unit_no) {
          moo_params->processEvent(m_this_unit, e, clock, -1, m_pxtn);
        }
      }
  } else {
    m_moo_state = std::make_unique<mooState>();
    pxtnVOMITPREPARATION prep{};
    prep.flags |= pxtnVOMITPREPFLAG_loop | pxtnVOMITPREPFLAG_unit_mute;
    prep.start_pos_sample = clock * 60 * 44100 /
                            m_pxtn->master->get_beat_clock() /
                            m_pxtn->master->get_beat_tempo();
    prep.master_volume = moo_params->master_vol;
    pxtn->moo_preparation(&prep, *m_moo_state);
    for (const EVERECORD *e = m_pxtn->evels->get_Records();
         e && e->clock <= clock; e = e->next) {
      if (e->unit_no < m_moo_state->units.size())
        moo_params->processEvent(&m_moo_state->units.at(e->unit_no), e, clock,
                                 -1, m_pxtn);
    }
    m_this_unit = &m_moo_state->units.at(unit_no);
  }

  for (const EVERECORD &e : additional_events)
    moo_params->processEvent(m_this_unit, &e, clock, -1, pxtn);
  m_this_unit->Tone_KeyOn();

  // We don't constantly reset because sometimes the audio engine forces
  // [life_count = 0] (say at the end of the sample)
  if (m_unit != nullptr) resetOn(duration);
  if (m_moo_state != nullptr) {
    for (auto &unit : m_moo_state->units) {
      pxtnUnitTone *u = &unit;
      std::shared_ptr<const pxtnWoice> woice =
          std::make_shared<const pxtnWoice>(u->get_woice());
      for (int i = 0; i < woice->get_voice_num(); ++i) {
        auto tone = u->get_tone(i);
        if (u == m_this_unit || tone->on_count > 0) {
          tone->on_count = duration;
          tone->life_count = duration + woice->get_instance(i)->env_release;
        }
      }
    }
  }

  if (device == nullptr) {
    device = new PxtoneUnitIODevice(nullptr, m_pxtn, moo_params);
    device->open(QIODevice::ReadOnly);
  }

  if (m_unit != nullptr) m_unit_ids.push_back(device->addUnit(m_unit.get()));
  if (m_moo_state != nullptr)
    for (uint i = 0; i < m_moo_state->units.size(); ++i)
      if (m_pxtn->Unit_Get(i)->get_played())
        m_unit_ids.push_back(device->addUnit(&m_moo_state->units[i]));

  if (audio == nullptr) {
    audio = new QAudioOutput(pxtoneAudioFormat(), device);
    audio->setVolume(1.0);
    audio->setBufferSize(bufferSize);
    audio->start(device);
  }
}

void NotePreview::processEvent(EVENTKIND kind, int32_t value) {
  m_moo_params->processNonOnEvent(m_this_unit, kind, value, m_pxtn);
}

void NotePreview::resetOn(int duration) {
  std::shared_ptr<const pxtnWoice> woice =
      std::make_shared<const pxtnWoice>(m_this_unit->get_woice());
  for (int i = 0; i < woice->get_voice_num(); ++i) {
    // TODO: calculating the life count should be more automatic.
    auto tone = m_this_unit->get_tone(i);
    *tone =
        pxtnVOICETONE{static_cast<double>(tone->env_release_clock),
                      tone->offset_freq, woice->get_instance(i)->env_size != 0};
    tone->on_count = duration;
    tone->life_count = duration + woice->get_instance(i)->env_release;
  }
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
                         int bufferSize, bool chordPreview, QObject *parent)
    : NotePreview(
          pxtn, moo_params, unit_no, clock,
          {ev(clock, EVENTKIND_KEY, pitch), ev(clock, EVENTKIND_VELOCITY, vel)},
          LONG_ON_VALUE, pxtn->Woice_Get(EVENTDEFAULT_VOICENO), bufferSize,
          chordPreview, parent) {}

NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int unit_no, int clock, int duration,
                         std::list<EVERECORD> additional_events, int bufferSize,
                         bool chordPreview, QObject *parent)
    : NotePreview(pxtn, moo_params, unit_no, clock, additional_events, duration,
                  pxtn->Woice_Get(EVENTDEFAULT_VOICENO), bufferSize,
                  chordPreview, parent){};

NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int unit_no, int clock,
                         std::list<EVERECORD> additional_events, int bufferSize,
                         bool chordPreview, QObject *parent)
    : NotePreview(pxtn, moo_params, unit_no, clock, LONG_ON_VALUE,
                  additional_events, bufferSize, chordPreview, parent){};

NotePreview::NotePreview(const pxtnService *pxtn, const mooParams *moo_params,
                         int pitch, int vel, int duration,
                         std::shared_ptr<const pxtnWoice> woice, int bufferSize,
                         QObject *parent)
    : NotePreview(pxtn, moo_params, -1, 0,
                  {ev(0, EVENTKIND_KEY, pitch), ev(0, EVENTKIND_VELOCITY, vel)},
                  duration, woice, bufferSize, false, parent) {}

NotePreview::~NotePreview() {
  for (const auto &id : m_unit_ids) device->removeUnit(id);
}
