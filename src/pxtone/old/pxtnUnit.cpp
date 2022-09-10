// '12/03/03

#include "./pxtnUnit.h"

#include "./pxtn.h"
#include "./pxtnEvelist.h"

pxtnUnit::pxtnUnit() {
  _bPlayed = true;
  _bOperated = false;
  _bVisible = true;
  strcpy(_name_buf, "no name");
  _name_size = int32_t(strlen(_name_buf));
}

pxtnUnit::~pxtnUnit() {}

pxtnUnitTone::pxtnUnitTone(std::shared_ptr<const pxtnWoice> p_woice) {
  _v_GROUPNO = EVENTDEFAULT_GROUPNO;
  _v_VELOCITY = EVENTDEFAULT_VELOCITY;
  _v_VOLUME = EVENTDEFAULT_VOLUME;
  _v_TUNING = EVENTDEFAULT_TUNING;
  _portament_sample_num = 0;
  _portament_sample_pos = 0;
  Tone_Clear();

  for (int32_t i = 0; i < pxtnMAX_CHANNEL; i++) {
    _pan_vols[i] = 64;
    _pan_times[i] = 0;
  }

  if (!set_woice(p_woice, true)) throw "Voice is null";
}

void pxtnUnitTone::Tone_Clear() {
  memset(_pan_time_bufs, 0,
         sizeof(int) * pxtnBUFSIZE_TIMEPAN * pxtnMAX_CHANNEL);
}

void pxtnUnitTone::Tone_Reset_and_2prm(int32_t voice_idx, int32_t env_rls_clock,
                                       float offset_freq) {
  pxtnVOICETONE *p_tone = &_vts[voice_idx];
  p_tone->life_count = 0;
  p_tone->on_count = 0;
  p_tone->smp_pos = 0;
  // p_tone->smooth_volume = 0;
  p_tone->env_release_clock = env_rls_clock;
  p_tone->offset_freq = offset_freq;
}

/* A custom way to initialize a pxtnVOICETONE for separate playing. Used to be
 * used for note previews, but now we just create a whole unit for previewing.
 */
void pxtnUnitTone::Tone_Reset_Custom(float tempo, float clock_rate,
                                     pxtnVOICETONE *vts) const {
  if (!_p_woice) return;
  const pxtnVOICEINSTANCE *p_inst;
  const pxtnVOICEUNIT *p_vc;
  std::shared_ptr<const pxtnWoice> p_wc = _p_woice;
  for (int32_t v = 0; v < p_wc->get_voice_num(); v++) {
    p_inst = p_wc->get_instance(v);
    p_vc = p_wc->get_voice(v);

    float ofs_freq = 0;
    if (p_vc->voice_flags & PTV_VOICEFLAG_BEATFIT) {
      ofs_freq = (p_inst->smp_body_w * tempo) / (44100 * 60 * p_vc->tuning);
    } else {
      ofs_freq =
          pxtnPulse_Frequency::Get(EVENTDEFAULT_BASICKEY - p_vc->basic_key) *
          p_vc->tuning;
    }
    vts[v] = pxtnVOICETONE((int32_t)(p_inst->env_release / clock_rate),
                           ofs_freq, p_inst->env_size != 0);
  }
}

void pxtnUnitTone::Tone_Reset(float tempo, float clock_rate) {
  Tone_Reset_Custom(tempo, clock_rate, _vts);
}

bool pxtnUnitTone::set_woice(std::shared_ptr<const pxtnWoice> p_woice,
                             bool resetKey) {
  if (!p_woice) return false;
  _p_woice = p_woice;
  if (resetKey) {
    _key_now = EVENTDEFAULT_KEY;
    _key_margin = 0;
    _key_start = EVENTDEFAULT_KEY;
  } else
    Tone_KeyOn();
  return true;
}

bool pxtnUnit::set_name_buf_jis(const char *name, int32_t buf_size) {
  if (!name || buf_size < 0 || buf_size > pxtnMAX_TUNEUNITNAME) return false;
  memset(_name_buf, 0, sizeof(_name_buf));
  if (buf_size) memcpy(_name_buf, name, buf_size);
  _name_size = buf_size;
  return true;
}

const char *pxtnUnit::get_name_buf_jis(int32_t *p_buf_size) const {
  if (p_buf_size) *p_buf_size = _name_size;
  return _name_buf;
}

bool pxtnUnit::is_name_buf() const {
  if (_name_size > 0) return true;
  return false;
}

void pxtnUnit::set_visible(bool b) { _bVisible = b; }
void pxtnUnit::set_operated(bool b) { _bOperated = b; }
void pxtnUnit::set_played(bool b) { _bPlayed = b; }

bool pxtnUnit::get_visible() const { return _bVisible; }
bool pxtnUnit::get_operated() const { return _bOperated; }
bool pxtnUnit::get_played() const { return _bPlayed; }

void pxtnUnitTone::Tone_ZeroLives() {
  for (int32_t i = 0; i < pxtnMAX_CHANNEL; i++) _vts[i].life_count = 0;
}

void pxtnUnitTone::Tone_KeyOn() {
  _key_now = _key_start + _key_margin;
  _key_start = _key_now;
  _key_margin = 0;
}

void pxtnUnitTone::Tone_Key(int32_t key) {
  _key_start = _key_now;
  _key_margin = key - _key_start;
  _portament_sample_pos = 0;
}

void pxtnUnitTone::Tone_Pan_Volume(int32_t ch, int32_t pan) {
  _pan_vols[0] = 64;
  _pan_vols[1] = 64;
  if (ch == 2) {
    if (pan >= 64)
      _pan_vols[0] = 128 - pan;
    else
      _pan_vols[1] = pan;
  }
}

void pxtnUnitTone::Tone_Pan_Time(int32_t ch, int32_t pan, int32_t sps) {
  _pan_times[0] = 0;
  _pan_times[1] = 0;

  if (ch == 2) {
    if (pan >= 64) {
      _pan_times[0] = pan - 64;
      if (_pan_times[0] > 63) _pan_times[0] = 63;
      _pan_times[0] = (_pan_times[0] * 44100) / sps;
    } else {
      _pan_times[1] = 64 - pan;
      if (_pan_times[1] > 63) _pan_times[1] = 63;
      _pan_times[1] = (_pan_times[1] * 44100) / sps;
    }
  }
}

void pxtnUnitTone::Tone_Velocity(int32_t val) { _v_VELOCITY = val; }
void pxtnUnitTone::Tone_Volume(int32_t val) { _v_VOLUME = val; }
void pxtnUnitTone::Tone_Portament(int32_t val) { _portament_sample_num = val; }
void pxtnUnitTone::Tone_GroupNo(int32_t val) { _v_GROUPNO = val; }
void pxtnUnitTone::Tone_Tuning(float val) { _v_TUNING = val; }
void pxtnUnitTone::Tone_Envelope_Custom(pxtnVOICETONE *vts) const {
  if (!_p_woice) return;

  /* In practice there are at most 2 voice nums */
  for (int32_t v = 0; v < _p_woice->get_voice_num(); v++) {
    const pxtnVOICEINSTANCE *p_vi = _p_woice->get_instance(v);
    pxtnVOICETONE *p_vt = &vts[v];

    if (p_vt->life_count > 0 && p_vi->env_size) {
      if (p_vt->on_count > 0) {
        if (p_vt->env_pos < p_vi->env_size) {
          p_vt->env_volume = p_vi->p_env[p_vt->env_pos];
          p_vt->env_pos++;
        }
      }
      // release.
      else {
        p_vt->env_volume = p_vt->env_start + (0 - p_vt->env_start) *
                                                 p_vt->env_pos /
                                                 p_vi->env_release;
        p_vt->env_pos++;
        // TODO: I think I can set life_count to 0 if env_pos > env_release.
        // But not sure.
      }
    }
  }
}
void pxtnUnitTone::Tone_Envelope() { Tone_Envelope_Custom(_vts); }

/* This sets up the buffers local to the unit for time pans (_pan_time_bufs)
 */
/* added [Tone_sample_custom] because [Tone_sample] by default modifies the
 * pxtnVOICETONE associated with the actual unit during playback. */
void pxtnUnitTone::Tone_Sample_Custom(int32_t ch_num, int32_t smooth_smp,
                                      pxtnVOICETONE *vts, int32_t *bufs) const {
  for (int32_t ch = 0; ch < pxtnMAX_CHANNEL; ch++) {
    int32_t time_pan_buf = 0;

    for (int32_t v = 0; v < _p_woice->get_voice_num(); v++) {
      /* tone represents configuration (e.g. wave offset) particular voice for
       * this unit */
      pxtnVOICETONE *p_vt = &vts[v];
      /* instance is the actual sample data */
      const pxtnVOICEINSTANCE *p_vi = _p_woice->get_instance(v);

      int32_t work = 0;

      if (p_vt->life_count > 0) {
        /* this smp_pos buffer alternates between left and right amps */
        /* Bytes: LLRRLLRR, increasing in time, I think. */
        int32_t pos = (int32_t)p_vt->smp_pos * 4 + ch * 2;
        work += *((short *)&p_vi->p_smp_w[pos]); /* syntax just means read the
                                                    thing at pos as short */

        /* if we're outputing to mono, get both L and R and avg */
        /* since this block will only be called to fill one buffer I think? */
        if (ch_num == 1) {
          work += *((short *)&p_vi->p_smp_w[pos + 2]);
          work = work / 2;
        }

        /* scaling filters */
        work = (work * _v_VELOCITY) / 128;
        work = (work * _v_VOLUME) / 128;
        work = work * _pan_vols[ch] / 64;

        if (p_vi->env_size)
          work = work * p_vt->env_volume / 128; /* ENVELOPE!! */

        // smooth tail
        if (_p_woice->get_voice(v)->voice_flags & PTV_VOICEFLAG_SMOOTH &&
            p_vt->life_count < smooth_smp) {
          work = work * p_vt->life_count / smooth_smp;
        }
      }
      time_pan_buf += work;
    }
    bufs[ch] = time_pan_buf;
  }
}

void pxtnUnitTone::Tone_Sample(bool b_mute, int32_t ch_num,
                               int32_t time_pan_index, int32_t smooth_smp) {
  if (!_p_woice) return;

  if (b_mute) {
    for (int32_t ch = 0; ch < ch_num; ch++)
      _pan_time_bufs[time_pan_index][ch] = 0;
    return;
  }

  Tone_Sample_Custom(ch_num, smooth_smp, _vts, _pan_time_bufs[time_pan_index]);
}

int32_t pxtnUnitTone::Tone_Supple_get(int32_t ch,
                                      int32_t time_pan_index) const {
  int32_t idx = (time_pan_index - _pan_times[ch]) & (pxtnBUFSIZE_TIMEPAN - 1);
  return _pan_time_bufs[idx][ch];
}
/* This dumps the time pan buffers into the group buffers */
void pxtnUnitTone::Tone_Supple(int32_t *group_smps, int32_t ch,
                               int32_t time_pan_index) const {
  group_smps[_v_GROUPNO] += Tone_Supple_get(ch, time_pan_index);
}

int pxtnUnitTone::Tone_Increment_Key() {
  // prtament..
  if (_portament_sample_num && _key_margin) {
    // 2021-08-06: comparing to sample_num - 1 instead of sample_num because in
    // some cases (after m20 on a 135bpm song) two portamentos right next to
    // each other cause a note jump b/c key_now ends up one sample away from
    // being set at the end of the first portamento
    if (_portament_sample_pos < _portament_sample_num - 1) {
      _portament_sample_pos++;
      _key_now =
          (int32_t)(_key_start + (double)_key_margin * _portament_sample_pos /
                                     _portament_sample_num);
    } else {
      _key_now = _key_start + _key_margin;
      _key_start = _key_now;
      _key_margin = 0;
    }
  } else {
    _key_now = _key_start + _key_margin;
  }
  return _key_now;
}

void pxtnUnitTone::Tone_Increment_Sample_Custom(float freq,
                                                pxtnVOICETONE *vts) const {
  if (!_p_woice) return;

  /* Up to two voices (the ones you see in ptvoice) */
  for (int32_t v = 0; v < _p_woice->get_voice_num(); v++) {
    const pxtnVOICEINSTANCE *p_vi = _p_woice->get_instance(v);
    pxtnVOICETONE *p_vt = &vts[v];

    if (p_vt->life_count > 0) p_vt->life_count--;
    if (p_vt->life_count > 0) {
      p_vt->on_count--;

      p_vt->smp_pos += p_vt->offset_freq * _v_TUNING * freq;

      if (p_vt->smp_pos >= p_vi->smp_body_w) {
        if (_p_woice->get_voice(v)->voice_flags & PTV_VOICEFLAG_WAVELOOP) {
          if (p_vt->smp_pos >= p_vi->smp_body_w)
            p_vt->smp_pos -= p_vi->smp_body_w;
          if (p_vt->smp_pos >= p_vi->smp_body_w) p_vt->smp_pos = 0;
        } else {
          p_vt->life_count = 0;
        }
      }

      // OFF
      if (p_vt->on_count == 0 && p_vi->env_size) {
        p_vt->env_start = p_vt->env_volume;
        p_vt->env_pos = 0;
      }
    }
  }
}

void pxtnUnitTone::Tone_Increment_Sample(float freq) {
  Tone_Increment_Sample_Custom(freq, _vts);
}

std::shared_ptr<const pxtnWoice> pxtnUnitTone::get_woice() const {
  return _p_woice;
}

pxtnVOICETONE *pxtnUnitTone::get_tone(int32_t voice_idx) {
  return &_vts[voice_idx];
}

// v1x (20byte) =================
typedef struct {
  char name[pxtnMAX_TUNEUNITNAME];
  uint16_t type;
  uint16_t group;
} _x1x_UNIT;

bool pxtnUnit::Read_v1x(pxtnDescriptor *p_doc, int32_t *p_group) {
  _x1x_UNIT unit;
  int32_t size;

  if (!p_doc->r(&size, 4, 1)) return false;
  if (!p_doc->r(&unit, sizeof(_x1x_UNIT), 1)) return false;
  if ((pxtnWOICETYPE)unit.type != pxtnWOICE_PCM) return false;

  memcpy(_name_buf, unit.name, pxtnMAX_TUNEUNITNAME);
  _name_buf[pxtnMAX_TUNEUNITNAME] = '\0';
  *p_group = unit.group;
  return true;
}

///////////////////
// pxtnUNIT x3x
///////////////////

typedef struct {
  uint16_t type;
  uint16_t group;
} _x3x_UNIT;

pxtnERR pxtnUnit::Read_v3x(pxtnDescriptor *p_doc, int32_t *p_group) {
  _x3x_UNIT unit{};
  int32_t size = 0;

  if (!p_doc->r(&size, 4, 1)) return pxtnERR_desc_r;
  if (!p_doc->r(&unit, sizeof(_x3x_UNIT), 1)) return pxtnERR_desc_r;
  if ((pxtnWOICETYPE)unit.type != pxtnWOICE_PCM &&
      (pxtnWOICETYPE)unit.type != pxtnWOICE_PTV &&
      (pxtnWOICETYPE)unit.type != pxtnWOICE_PTN)
    return pxtnERR_fmt_unknown;
  *p_group = unit.group;

  return pxtnOK;
}
