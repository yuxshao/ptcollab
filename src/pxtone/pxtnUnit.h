// '12/03/03

#ifndef pxtnUnit_H
#define pxtnUnit_H

#include "./pxtn.h"
#include "./pxtnDescriptor.h"
#include "./pxtnMax.h"
#include "./pxtnWoice.h"

/// Note: I extracted out the stuff related to playing a unit into a separate
/// struct, so that this can be outside of the pxtnService state.
class pxtnUnitTone {
 private:
  //    TUNEUNITTONESTRUCT
  int32_t _key_now;
  int32_t _key_start;
  int32_t _key_margin;
  int32_t _portament_sample_pos;
  int32_t _portament_sample_num;
  int32_t _pan_vols[pxtnMAX_CHANNEL];
  int32_t _pan_times[pxtnMAX_CHANNEL];

  /* Flipped the row-col order here so that Tone_Sample_Custom is easier */
  int32_t _pan_time_bufs[pxtnBUFSIZE_TIMEPAN][pxtnMAX_CHANNEL];
  int32_t _v_VOLUME;
  int32_t _v_VELOCITY;
  int32_t _v_GROUPNO;
  float _v_TUNING;

  std::shared_ptr<const pxtnWoice> _p_woice;

  pxtnVOICETONE _vts[pxtnMAX_UNITCONTROLVOICE];

 public:
  pxtnUnitTone(std::shared_ptr<const pxtnWoice> p_woice);

  void Tone_Clear();

  void Tone_Reset_and_2prm(int32_t voice_idx, int32_t env_rls_clock,
                           float offset_freq);
  void Tone_Reset_Custom(float tempo, float clock_rate,
                         pxtnVOICETONE *vts) const;
  void Tone_Reset(float tempo, float clock_rate);
  void Tone_Envelope_Custom(pxtnVOICETONE *vts) const;
  void Tone_Envelope();
  void Tone_KeyOn();
  void Tone_ZeroLives();
  void Tone_Key(int32_t key);
  void Tone_Pan_Volume(int32_t ch, int32_t pan);
  void Tone_Pan_Time(int32_t ch, int32_t pan, int32_t sps);

  void Tone_Velocity(int32_t val);
  void Tone_Volume(int32_t val);
  void Tone_Portament(int32_t val);
  void Tone_GroupNo(int32_t val);
  void Tone_Tuning(float val);

  void Tone_Sample_Custom(int32_t ch_num, int32_t smooth_smp,
                          pxtnVOICETONE *vts, int32_t *bufs) const;
  void Tone_Sample(bool b_mute, int32_t ch_num, int32_t time_pan_index,
                   int32_t smooth_smp);
  int32_t Tone_Supple_get(int32_t ch, int32_t time_pan_index) const;
  void Tone_Supple(int32_t *group_smps, int32_t ch_num,
                   int32_t time_pan_index) const;
  int32_t Tone_Increment_Key();
  void Tone_Increment_Sample_Custom(float freq, pxtnVOICETONE *vts) const;
  void Tone_Increment_Sample(float freq);

  bool set_woice(std::shared_ptr<const pxtnWoice> p_woice, bool resetKey);
  std::shared_ptr<const pxtnWoice> get_woice() const;

  pxtnVOICETONE *get_tone(int32_t voice_idx);
};

class pxtnUnit {
 private:
  void operator=(const pxtnUnit &src) = delete;
  pxtnUnit(const pxtnUnit &src) = delete;

  bool _bVisible;
  bool _bOperated;
  bool _bPlayed;
  char _name_buf[pxtnMAX_TUNEUNITNAME + 1];
  int32_t _name_size;

 public:
  pxtnUnit();
  ~pxtnUnit();

  bool set_name_buf_jis(const char *name_buf, int32_t buf_size);
  const char *get_name_buf_jis(int32_t *p_buf_size) const;
  bool is_name_buf() const;

  void set_visible(bool b);
  void set_operated(bool b);
  void set_played(bool b);
  bool get_visible() const;
  bool get_operated() const;
  bool get_played() const;

  pxtnERR Read_v3x(pxtnDescriptor *p_doc, int32_t *p_group);
  bool Read_v1x(pxtnDescriptor *p_doc, int32_t *p_group);
};

#endif
