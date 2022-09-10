// '12/03/03

#ifndef pxtnDelay_H
#define pxtnDelay_H

#include <vector>

#include "./pxtn.h"
#include "./pxtnDescriptor.h"
#include "./pxtnMax.h"

enum DELAYUNIT : int8_t {
  DELAYUNIT_Beat = 0,
  DELAYUNIT_Meas,
  DELAYUNIT_Second,
  DELAYUNIT_max = DELAYUNIT_Second,
};
extern const char* DELAYUNIT_name(DELAYUNIT unit);

// TODO: Make this POD?
class pxtnDelay {
 private:
  bool _b_played;
  DELAYUNIT _unit;
  int32_t _group;
  float _rate;
  float _freq;

 public:
  pxtnDelay();

  bool Write(pxtnDescriptor* p_doc) const;
  pxtnERR Read(pxtnDescriptor* p_doc);

  DELAYUNIT get_unit() const;
  float get_freq() const;
  float get_rate() const;
  int32_t get_group() const;

  void Set(DELAYUNIT unit, float freq, float rate, int32_t group);

  bool get_played() const;
  void set_played(bool b);
  bool switch_played();
};

class pxtnDelayTone {
 private:
  int32_t _smp_num;
  int32_t _offset;
  std::unique_ptr<int32_t[]> _bufs[pxtnMAX_CHANNEL];
  int32_t _rate_s32;

 public:
  pxtnDelayTone(const pxtnDelay& delay, int32_t beat_num, float beat_tempo,
                int32_t sps);
  void Tone_Supple(const pxtnDelay& delay, int32_t ch_num, int32_t* group_smps);
  void Tone_Increment();
  void Tone_Clear();
};

#endif
