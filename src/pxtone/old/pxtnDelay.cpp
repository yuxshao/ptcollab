
#include "./pxtnDelay.h"

#include "./pxtn.h"
#include "./pxtnMax.h"
#include "./pxtnMem.h"

const char *DELAYUNIT_names[] = {"Beat", "Meas", "Sec."};
const char *DELAYUNIT_name(DELAYUNIT unit) {
  if (unit > DELAYUNIT_max) return "unknown";
  return DELAYUNIT_names[unit];
}

pxtnDelay::pxtnDelay() {
  _b_played = true;
  _unit = DELAYUNIT_Beat;
  _group = 0;
  _rate = 33.0;
  _freq = 3.f;
}

DELAYUNIT pxtnDelay::get_unit() const { return _unit; }
int32_t pxtnDelay::get_group() const { return _group; }
float pxtnDelay::get_rate() const { return _rate; }
float pxtnDelay::get_freq() const { return _freq; }

void pxtnDelay::Set(DELAYUNIT unit, float freq, float rate, int32_t group) {
  _unit = unit;
  _group = group;
  _rate = rate;
  _freq = freq;
}

bool pxtnDelay::get_played() const { return _b_played; }
void pxtnDelay::set_played(bool b) { _b_played = b; }
bool pxtnDelay::switch_played() {
  _b_played = _b_played ? false : true;
  return _b_played;
}

pxtnDelayTone::pxtnDelayTone(const pxtnDelay &delay, int32_t beat_num,
                             float beat_tempo, int32_t sps) {
  _smp_num = 0;
  _offset = 0;
  _rate_s32 = 100;

  if (delay.get_freq() && delay.get_rate()) {
    _offset = 0;
    _rate_s32 = (int32_t)delay.get_rate();  // /100;

    switch (delay.get_unit()) {
      case DELAYUNIT_Beat:
        _smp_num = (int32_t)(sps * 60 / beat_tempo / delay.get_freq());
        break;
      case DELAYUNIT_Meas:
        _smp_num =
            (int32_t)(sps * 60 * beat_num / beat_tempo / delay.get_freq());
        break;
      case DELAYUNIT_Second:
        _smp_num = (int32_t)(sps / delay.get_freq());
        break;
    }

    for (int32_t c = 0; c < pxtnMAX_CHANNEL; c++)
      _bufs[c] = std::make_unique<int32_t[]>(_smp_num);
    Tone_Clear();
  }
}

void pxtnDelayTone::Tone_Supple(const pxtnDelay &delay, int32_t ch,
                                int32_t *group_smps) {
  if (!_smp_num) return;
  int32_t a = _bufs[ch][_offset] * _rate_s32 / 100;
  if (delay.get_played()) group_smps[delay.get_group()] += a;
  _bufs[ch][_offset] = group_smps[delay.get_group()];
}

void pxtnDelayTone::Tone_Increment() {
  if (!_smp_num) return;
  if (++_offset >= _smp_num) _offset = 0;
}

void pxtnDelayTone::Tone_Clear() {
  if (!_smp_num) return;
  int32_t def = 0;  // ..
  for (int32_t i = 0; i < pxtnMAX_CHANNEL; i++)
    memset(_bufs[i].get(), def, _smp_num * sizeof(int32_t));
}

// (12byte) =================
typedef struct {
  uint16_t unit;
  uint16_t group;
  float rate;
  float freq;
} _DELAYSTRUCT;

bool pxtnDelay::Write(pxtnDescriptor *p_doc) const {
  _DELAYSTRUCT dela;
  int32_t size;

  memset(&dela, 0, sizeof(_DELAYSTRUCT));
  dela.unit = (uint16_t)_unit;
  dela.group = (uint16_t)_group;
  dela.rate = _rate;
  dela.freq = _freq;

  // dela ----------
  size = sizeof(_DELAYSTRUCT);
  if (!p_doc->w_asfile(&size, sizeof(int32_t), 1)) return false;
  if (!p_doc->w_asfile(&dela, size, 1)) return false;

  return true;
}

pxtnERR pxtnDelay::Read(pxtnDescriptor *p_doc) {
  _DELAYSTRUCT dela{};
  int32_t size = 0;

  if (!p_doc->r(&size, 4, 1)) return pxtnERR_desc_r;
  if (!p_doc->r(&dela, sizeof(_DELAYSTRUCT), 1)) return pxtnERR_desc_r;
  if (dela.unit > DELAYUNIT_max) return pxtnERR_fmt_unknown;

  _unit = (DELAYUNIT)dela.unit;
  _freq = dela.freq;
  _rate = dela.rate;
  _group = dela.group;

  if (_group >= pxtnMAX_TUNEGROUPNUM) _group = 0;

  return pxtnOK;
}
