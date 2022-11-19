
#include "../editor/audio/VolumeMeter.h"
#include "./pxtn.h"
#include "./pxtnMem.h"
#include "./pxtnService.h"

mooParams::mooParams() {
  b_mute_by_unit = false;
  b_loop = true;

  master_vol = 1.0f;
}

mooState::mooState() {
  p_eve = NULL;
  num_loop = 0;
  smp_count = 0;
  fade_fade = 0;
  end_vomit = true;
}

void mooState::resetGroups(int32_t group_num) {
  group_smps.clear();
  group_smps.resize(group_num, 0);
}

bool mooState::resetUnits(size_t unit_num,
                          std::shared_ptr<const pxtnWoice> woice) {
  units.clear();
  units.reserve(unit_num);
  for (size_t i = 0; i < unit_num; ++i)
    if (!addUnit(woice)) return false;
  return true;
}

bool mooState::addUnit(std::shared_ptr<const pxtnWoice> woice) {
  if (!woice) return false;
  units.emplace_back(woice);
  params.resetVoiceOn(&(*units.rbegin()));
  return true;
}

////////////////////////////////////////////////
// Units   ////////////////////////////////////
////////////////////////////////////////////////

void mooParams::resetVoiceOn(pxtnUnitTone* p_u) const {
  p_u->Tone_Reset(bt_tempo, clock_rate);
}

bool pxtnService::_moo_InitUnitTone(mooState& moo_state) const {
  return moo_state.resetUnits(_unit_num, Woice_Get(EVENTDEFAULT_VOICENO));
}
#include <QDebug>
void mooParams::processNonOnEvent(pxtnUnitTone* p_u, EVENTKIND kind,
                                  int32_t value,
                                  const pxtnService* pxtn) const {
  int32_t dst_ch_num, dst_sps;
  pxtn->get_destination_quality(&dst_ch_num, &dst_sps);
  switch (kind) {
    case EVENTKIND_KEY:
      p_u->Tone_Key(value);
      break;
    case EVENTKIND_PAN_VOLUME:
      p_u->Tone_Pan_Volume(dst_ch_num, value);
      break;
    case EVENTKIND_PAN_TIME:
      p_u->Tone_Pan_Time(dst_ch_num, value, dst_sps);
      break;
    case EVENTKIND_VELOCITY:
      p_u->Tone_Velocity(value);
      break;
    case EVENTKIND_VOLUME:
      p_u->Tone_Volume(value);
      break;
    case EVENTKIND_PORTAMENT:
      p_u->Tone_Portament((int32_t)(value * clock_rate));
      break;
    case EVENTKIND_BEATCLOCK:
    case EVENTKIND_BEATTEMPO:
    case EVENTKIND_BEATNUM:
    case EVENTKIND_REPEAT:
    case EVENTKIND_LAST:
    case EVENTKIND_NULL:
    case EVENTKIND_NUM:
      break;
    case EVENTKIND_VOICENO: {
      /// Normally setting a woice resets the key, but this messes up some note
      /// previews. Use the sign of [value] to signal whether or not to reset
      /// the key.
      p_u->set_woice(pxtn->Woice_Get(value >= 0 ? value : -value - 1),
                     (value >= 0));
      resetVoiceOn(p_u);
    } break;
    case EVENTKIND_GROUPNO:
      p_u->Tone_GroupNo(value);
      break;
    case EVENTKIND_TUNING:
      p_u->Tone_Tuning(*((float*)(&value)));
      break;
    case EVENTKIND_ON: {
      // A bit hacky but interpret EVENTKIND_ON value as how much time is left
      std::shared_ptr<const pxtnWoice> p_wc;
      if (!(p_wc = p_u->get_woice())) break;
      for (int32_t v = 0; v < p_wc->get_voice_num(); v++) {
        pxtnVOICETONE* p_tone = p_u->get_tone(v);
        const pxtnVOICEINSTANCE* p_vi = p_wc->get_instance(v);
        p_tone->life_count = p_vi->env_release + value;  //
        p_tone->on_count = value;
      }
    }
  }
}

// u is used to look ahead to cut short notes whose release go into the next.
// This note duration cutting is for the smoothing near the end of a note.
void mooParams::processEvent(pxtnUnitTone* p_u, const EVERECORD* e,
                             int32_t clock, int32_t smp_end,
                             const pxtnService* pxtn) const {
  pxtnVOICETONE* p_tone;
  std::shared_ptr<const pxtnWoice> p_wc;
  const pxtnVOICEINSTANCE* p_vi;

  switch (e->kind) {
    case EVENTKIND_ON: {
      int32_t on_count = (int32_t)((e->clock + e->value - clock) * clock_rate);
      if (on_count <= 0) {
        p_u->Tone_ZeroLives();
        break;
      }

      p_u->Tone_KeyOn();

      if (!(p_wc = p_u->get_woice())) break;
      for (int32_t v = 0; v < p_wc->get_voice_num(); v++) {
        p_tone = p_u->get_tone(v);
        p_vi = p_wc->get_instance(v);

        // release..
        if (p_vi->env_release) {
          /* the actual length of the note + release. the (clock - ..->clock)
           * is in case we skip start */
          int32_t max_life_count1 =
              (int32_t)((e->value - (clock - e->clock)) * clock_rate) +
              p_vi->env_release;
          int32_t max_life_count2;
          int32_t c = e->clock + e->value + p_tone->env_release_clock;
          EVERECORD* next = NULL;
          for (EVERECORD* p = e->next; p; p = p->next) {
            if (p->clock > c) break;
            if (p->unit_no == e->unit_no && p->kind == EVENTKIND_ON) {
              next = p;
              break;
            }
          }
          /* end the note at the end of the song if there's no next note */
          if (!next) {
            if (smp_end == -1)
              max_life_count2 = max_life_count1;
            else
              max_life_count2 = smp_end - (int32_t)(clock * clock_rate);
            /* end the note at the next note otherwise. */
          } else
            max_life_count2 = (int32_t)((next->clock - clock) * clock_rate);
          /* finally, take min of both */
          if (max_life_count1 < max_life_count2)
            p_tone->life_count = max_life_count1;
          else
            p_tone->life_count = max_life_count2;
        }
        // no-release..
        else {
          p_tone->life_count =
              (int32_t)((e->value - (clock - e->clock)) * clock_rate);
        }

        if (p_tone->life_count > 0) {
          p_tone->on_count = on_count;
          p_tone->smp_pos = 0;
          p_tone->env_pos = 0;
          if (p_vi->env_size)
            p_tone->env_volume = p_tone->env_start = 0;  // envelope
          else
            p_tone->env_volume = p_tone->env_start = 128;  // no-envelope
        }
      }
      break;
    }

    default:
      processNonOnEvent(p_u, EVENTKIND(e->kind), e->value, pxtn);
      break;
  }
}
#include <QDebug>
// TODO: Could probably put this in moo_state. Maybe make moo_state.params a
// member of it.
bool pxtnService::_moo_PXTONE_SAMPLE(void* p_data, mooState& moo_state) const {
  // envelope..
  for (size_t u = 0; u < moo_state.units.size(); u++)
    moo_state.units[u].Tone_Envelope();

  int32_t clock = (int32_t)(moo_state.smp_count / moo_state.params.clock_rate);

  /* Adding constant update to moo_smp_end since we might be editing while
   * playing */
  int32_t smp_end = ((double)master->get_play_meas() * master->get_beat_num() *
                     master->get_beat_clock() * moo_state.params.clock_rate);

  /* Notify all the units of events that occurred since the last time
     increment and adjust sampling parameters accordingly */
  // events..

  // Keep the last played event in moo_state, so that if a new event pops up
  // during playback at the end, it's not missed.
  // Handling arbitrary changes while playing is a bit more difficult. You'd
  // have to split by event type at least, since something near the beginning
  // could have lasting effects to now.
  const EVERECORD* next =
      (moo_state.p_eve ? moo_state.p_eve->next : evels->get_Records());
  while (next && next->clock <= clock) {
    int32_t u = next->unit_no;
    // TODO: Be robust to if there's a mention of a new unit. Generate the new
    // unit on the fly? (update: currently done by adding in the controller)
    moo_state.params.processEvent(&moo_state.units[u], next, clock, smp_end,
                                  this);
    moo_state.p_eve = next;
    next = moo_state.p_eve->next;
  }

  // sampling..
  for (size_t u = 0; u < moo_state.units.size(); u++) {
    bool muted;
    if (moo_state.params.solo_unit.has_value())
      muted = moo_state.params.solo_unit.value() != u;
    else
      muted = moo_state.params.b_mute_by_unit && !_units[u]->get_played();
    moo_state.units[u].Tone_Sample(muted, _dst_ch_num, moo_state.time_pan_index,
                                   moo_state.params.smp_smooth);
  }

  for (int32_t ch = 0; ch < _dst_ch_num; ch++) {
    for (int32_t g = 0; g < _group_num; g++) moo_state.group_smps[g] = 0;
    /* Sample the units into a group buffer */
    for (size_t u = 0; u < moo_state.units.size(); u++)
      moo_state.units[u].Tone_Supple(moo_state.group_smps.data(), ch,
                                     moo_state.time_pan_index);
    /* Add overdrive, delay to group buffer */
    for (size_t o = 0; o < _ovdrvs.size(); o++)
      _ovdrvs[o].Tone_Supple(moo_state.group_smps.data());
    for (size_t d = 0; d < _delays.size(); d++) {
      // TODO: Be robust to if there's a new delay. Generate new delay on the
      // fly?
      moo_state.delays[d].Tone_Supple(_delays[d], ch,
                                      moo_state.group_smps.data());
    }

    /* Add group samples together for final */
    // collect.
    int32_t work = 0;
    for (int32_t g = 0; g < _group_num; g++) work += moo_state.group_smps[g];

    /* Fading scale probably for rendering at the end */
    // fade..
    if (moo_state.fade_fade)
      work = work * (moo_state.fade_count >> 8) / moo_state.fade_max;

    // master volume
    work = (int32_t)(work * moo_state.params.master_vol);

    // to buffer..
    if (work > moo_state.params.top) work = moo_state.params.top;
    if (work < -moo_state.params.top) work = -moo_state.params.top;
    *((int16_t*)p_data + ch) = (int16_t)(work);
  }

  // --------------
  // increments..

  moo_state.smp_count++;
  moo_state.time_pan_index =
      (moo_state.time_pan_index + 1) & (pxtnBUFSIZE_TIMEPAN - 1);

  for (size_t u = 0; u < moo_state.units.size(); u++) {
    int32_t key_now = moo_state.units[u].Tone_Increment_Key();
    moo_state.units[u].Tone_Increment_Sample(
        pxtnPulse_Frequency::Get2(key_now) * moo_state.params.smp_stride);
  }

  // delay
  for (size_t d = 0; d < moo_state.delays.size(); d++)
    moo_state.delays[d].Tone_Increment();

  // fade out
  if (moo_state.fade_fade < 0) {
    if (moo_state.fade_count > 0)
      moo_state.fade_count--;
    else
      return false;
  }
  // fade in
  else if (moo_state.fade_fade > 0) {
    if (moo_state.fade_count < (moo_state.fade_max << 8))
      moo_state.fade_count++;
    else
      moo_state.fade_fade = 0;
  }

  while (moo_state.smp_count >= smp_end) {
    if (!moo_state.params.b_loop) return false;
    ++moo_state.num_loop;
    moo_state.smp_count -= smp_end;
    moo_state.smp_count +=
        master->get_this_clock(master->get_repeat_meas(), 0, 0) *
        moo_state.params.clock_rate;
    moo_state.p_eve = nullptr;
    _moo_InitUnitTone(moo_state);
  }
  return true;
}

///////////////////////
// get / set
///////////////////////

#include <QDebug>
int32_t pxtnService::moo_tone_sample_multi(std::map<int, pxtnUnitTone*> p_us,
                                           const mooParams& moo_params,
                                           void* data, int32_t buf_size,
                                           int32_t time_pan_index) const {
  // TODO: Try to deduplicate this with _moo_PXTONE_SAMPLE
  if (buf_size < _dst_ch_num) return 0;

  for (auto& [id, p_u] : p_us) {
    if (!p_u) return 0;
    p_u->Tone_Sample(false, _dst_ch_num, time_pan_index, moo_params.smp_smooth);
    int32_t key_now = p_u->Tone_Increment_Key();
    p_u->Tone_Increment_Sample(pxtnPulse_Frequency::Get2(key_now) *
                               moo_params.smp_stride);
  }
  for (int ch = 0; ch < _dst_ch_num; ++ch) {
    int32_t work = 0;
    for (auto& [id, p_u] : p_us)
      work += p_u->Tone_Supple_get(ch, time_pan_index);
    work *= moo_params.master_vol;
    if (work > moo_params.top) work = moo_params.top;
    if (work < -moo_params.top) work = -moo_params.top;
    *((int16_t*)data + ch) += (int16_t)(work);
  }

  return _dst_ch_num * sizeof(int16_t);
}

bool pxtnService::moo_is_valid_data() const { return _moo_b_valid_data; }

/* This place might be a chance to allow variable tempo songs */
int32_t pxtnService::moo_get_now_clock(const mooState& moo_state) const {
  if (moo_state.params.clock_rate)
    return (int32_t)(moo_state.smp_count / moo_state.params.clock_rate);
  return 0;
}

int32_t pxtnService::moo_get_end_clock() const {
  return master->get_this_clock(master->get_play_meas(), 0, 0);
}

bool pxtnService::moo_set_fade(int32_t fade, float sec,
                               mooState& moo_state) const {
  moo_state.fade_max = (int32_t)((float)_dst_sps * sec) >> 8;
  if (fade < 0) {
    moo_state.fade_fade = -1;
    moo_state.fade_count = moo_state.fade_max << 8;
  }  // out
  else if (fade > 0) {
    moo_state.fade_fade = 1;
    moo_state.fade_count = 0;
  }  // in
  else {
    moo_state.fade_fade = 0;
    moo_state.fade_count = 0;
  }  // off
  return true;
}

////////////////////////////
// preparation
////////////////////////////

// preparation
bool pxtnService::moo_preparation(const pxtnVOMITPREPARATION* p_prep,
                                  mooState& moo_state) const {
  if (!_moo_b_valid_data || !_dst_ch_num || !_dst_sps || !_dst_byte_per_smp) {
    moo_state.end_vomit = true;
    return false;
  }

  bool b_ret = false;
  int32_t start_meas = 0;
  int32_t start_sample = 0;
  float start_float = 0;

  float fadein_sec = 0;

  if (p_prep) {
    start_meas = p_prep->start_pos_meas;
    start_sample = p_prep->start_pos_sample;
    start_float = p_prep->start_pos_float;

    // TODO: Maybe put them to use?
    // if (p_prep->meas_end) meas_end = p_prep->meas_end;
    // if (p_prep->meas_repeat) meas_repeat = p_prep->meas_repeat;
    if (p_prep->fadein_sec) fadein_sec = p_prep->fadein_sec;

    if (p_prep->flags & pxtnVOMITPREPFLAG_unit_mute)
      moo_state.params.b_mute_by_unit = true;
    else
      moo_state.params.b_mute_by_unit = false;
    if (p_prep->flags & pxtnVOMITPREPFLAG_loop)
      moo_state.params.b_loop = true;
    else
      moo_state.params.b_loop = false;

    moo_state.params.master_vol = p_prep->master_volume;
    moo_state.params.solo_unit = p_prep->solo_unit;
  }

  /* _dst_sps is like samples to seconds. it's set in pxtnService.cpp
     constructor, comes from Main.cpp. 44100 is passed to both this & xaudio.
   */
  /* samples per clock tick */
  moo_state.params.clock_rate = (float)(60.0f * (double)_dst_sps /
                                        ((double)master->get_beat_tempo() *
                                         (double)master->get_beat_clock()));
  moo_state.params.bt_tempo = master->get_beat_tempo();
  moo_state.params.smp_stride = (44100.0f / _dst_sps);
  moo_state.params.top = 0x7fff;

  moo_state.time_pan_index = 0;
  moo_state.resetGroups(_group_num);

  int32_t smp_start;
  if (start_float)
    smp_start = (int32_t)((float)moo_get_total_sample() * start_float);
  else if (start_sample)
    smp_start = start_sample;
  else
    smp_start =
        master->get_this_clock(start_meas, 0, 0) * moo_state.params.clock_rate;

  moo_state.smp_count = smp_start;
  moo_state.params.smp_smooth = _dst_sps / 250;  // (0.004sec) // (0.010sec)

  if (fadein_sec > 0)
    moo_set_fade(1, fadein_sec, moo_state);
  else
    moo_set_fade(0, 0, moo_state);

  moo_state.tones_clear();

  moo_state.p_eve = nullptr;
  moo_state.num_loop = 0;

  _moo_InitUnitTone(moo_state);

  b_ret = true;
  moo_state.end_vomit = false;

  return b_ret;
}

int32_t pxtnService::moo_get_total_sample() const {
  if (!_b_init) return 0;
  if (!_moo_b_valid_data) return 0;

  int32_t meas_num;
  int32_t beat_num;
  float beat_tempo;
  master->Get(&beat_num, &beat_tempo, NULL, &meas_num);
  return pxtnService_moo_CalcSampleNum(meas_num, beat_num, _dst_sps,
                                       master->get_beat_tempo());
}

////////////////////
// Moo ...
////////////////////

bool pxtnService::Moo(
    mooState& moo_state, void* p_buf, int32_t size, int32_t* filled_size,
    std::vector<InterpolatedVolumeMeter>* volume_meters) const {
  if (filled_size) *filled_size = 0;

  if (!_moo_b_valid_data) return false;
  if (moo_state.end_vomit) return false;

  bool b_ret = false;

  int32_t smp_w = 0;

  // Testing what happens if mooing takes a long time
  // for (int i = 0, j = 0; i < 20000000; ++i) j += i * i;
  /* No longer failing on remainder - we just return the filled size */
  // if( size % _dst_byte_per_smp ) return false;

  /* Size/smp_num probably is used to sync the playback with the position */
  int32_t smp_num = size / _dst_byte_per_smp;

  {
    /* Buffer is renamed here */
    int16_t* p16 = (int16_t*)p_buf;
    int16_t sample[2]; /* for left and right? */

    /* Iterate thru samples [smp_num] times, fill buffer with this sample */
    for (smp_w = 0; smp_w < smp_num; smp_w++) {
      if (!_moo_PXTONE_SAMPLE(sample, moo_state)) {
        moo_state.end_vomit = true;
        break;
      }
      for (int ch = 0; ch < _dst_ch_num; ch++, p16++) {
        if (volume_meters) (*volume_meters)[ch].insert(sample[ch]);
        qDebug() << sample[ch];
        *p16 = sample[ch];
      }
    }
    for (; smp_w < smp_num; smp_w++) {
      for (int ch = 0; ch < _dst_ch_num; ch++, p16++) *p16 = 0;
    }

    if (filled_size) *filled_size = smp_num * _dst_byte_per_smp;
  }

  if (_sampled_proc) {
    if (!_sampled_proc(_sampled_user, this)) {
      moo_state.end_vomit = true;
      goto term;
    }
  }

  b_ret = true;
term:
  return b_ret;
}

int32_t pxtnService_moo_CalcSampleNum(int32_t meas_num, int32_t beat_num,
                                      int32_t sps, float beat_tempo) {
  uint32_t total_beat_num;
  uint32_t sample_num;
  if (!beat_tempo) return 0;
  total_beat_num = meas_num * beat_num;
  sample_num = (uint32_t)((double)sps * 60 * (double)total_beat_num /
                          (double)beat_tempo);
  return sample_num;
}
