#ifndef pxtnService_H
#define pxtnService_H

#include "./pxtn.h"
#include "./pxtnDelay.h"
#include "./pxtnDescriptor.h"
#include "./pxtnEvelist.h"
#include "./pxtnMaster.h"
#include "./pxtnMax.h"
#include "./pxtnOverDrive.h"
#include "./pxtnPulse_NoiseBuilder.h"
#include "./pxtnText.h"
#include "./pxtnUnit.h"
#include "./pxtnWoice.h"

#define PXTONEERRORSIZE 64

#define pxtnVOMITPREPFLAG_loop 0x01
#define pxtnVOMITPREPFLAG_unit_mute 0x02

typedef struct {
  int32_t start_pos_meas;
  int32_t start_pos_sample;
  float start_pos_float;

  int32_t meas_end;
  int32_t meas_repeat;
  float fadein_sec;

  uint32_t flags;
  float master_volume;
} pxtnVOMITPREPARATION;

class pxtnService;

// Static parameters that are computed when moo is initialized.
struct mooParams {
  // Whether muting individual units is allowed or not
  bool b_mute_by_unit;
  // Is looping enabled?
  bool b_loop;

  // How many samples at the tail end of the note to use to fade to silence?
  int32_t smp_smooth;
  float clock_rate;    // as the sample
  int32_t smp_start;   // Where was this moo initialized?
  int32_t smp_end;     // What's the last sample number in the song?
  int32_t smp_repeat;  // What sample # is the repeat?

  // Some fade params. Not looked into it much yet
  int32_t fade_count;
  int32_t fade_max;
  int32_t fade_fade;
  float master_vol;

  int32_t top;  // max pcm value allowed
  float smp_stride;

  float bt_tempo;

  // for make now-meas
  int32_t bt_clock;
  int32_t bt_num;

  mooParams();

  void processEvent(pxtnUnit *p_u, int32_t u, const EVERECORD *e, int32_t clock,
                    int32_t dst_ch_num, int32_t dst_sps,
                    const pxtnService *pxtn) const;

  // TODO: maybe don't need to expose
  bool resetVoiceOn(pxtnUnit *p_u, int32_t w, const pxtnService *pxtn) const;
};

// Moo values that change as the song plays.
struct mooState {
  // Buffers that units write to for group operations
  int32_t *group_smps;
  int32_t time_pan_index;

  // Current sample position
  int32_t smp_count;

  // Next event
  const EVERECORD *p_eve;

  mooState();

  void release();

  bool init(int32_t group_num);
};

typedef bool (*pxtnSampledCallback)(void *user, const pxtnService *pxtn);

class pxtnService {
 private:
  void operator=(const pxtnService &src) = delete;
  pxtnService(const pxtnService &src) = delete;

  enum _enum_FMTVER {
    _enum_FMTVER_unknown = 0,
    _enum_FMTVER_x1x,  // fix event num = 10000
    _enum_FMTVER_x2x,  // no version of exe
    _enum_FMTVER_x3x,  // unit has voice / basic-key for only view
    _enum_FMTVER_x4x,  // unit has event
    _enum_FMTVER_v5,
  };

  bool _b_init;
  bool _b_edit;
  bool _b_fix_evels_num;

  int32_t _dst_ch_num, _dst_sps, _dst_byte_per_smp;

  pxtnPulse_NoiseBuilder *_ptn_bldr;

  int32_t _delay_max;
  int32_t _delay_num;
  pxtnDelay **_delays;
  int32_t _ovdrv_max;
  int32_t _ovdrv_num;
  pxtnOverDrive **_ovdrvs;
  int32_t _woice_max;
  int32_t _woice_num;
  pxtnWoice **_woices;
  int32_t _unit_max;
  int32_t _unit_num;
  // TODO: The units are largely also part of the moo state, besides the name.
  pxtnUnit **_units;

  int32_t _group_num;

  pxtnERR _ReadVersion(pxtnDescriptor *p_doc, _enum_FMTVER *p_fmt_ver,
                       uint16_t *p_exe_ver);
  pxtnERR _ReadTuneItems(pxtnDescriptor *p_doc);
  bool _x1x_Project_Read(pxtnDescriptor *p_doc);

  pxtnERR _io_Read_Delay(pxtnDescriptor *p_doc);
  pxtnERR _io_Read_OverDrive(pxtnDescriptor *p_doc);
  pxtnERR _io_Read_Woice(pxtnDescriptor *p_doc, pxtnWOICETYPE type);
  pxtnERR _io_Read_OldUnit(pxtnDescriptor *p_doc, int32_t ver);

  bool _io_assiWOIC_w(pxtnDescriptor *p_doc, int32_t idx) const;
  pxtnERR _io_assiWOIC_r(pxtnDescriptor *p_doc);
  bool _io_assiUNIT_w(pxtnDescriptor *p_doc, int32_t idx) const;
  pxtnERR _io_assiUNIT_r(pxtnDescriptor *p_doc);

  bool _io_UNIT_num_w(pxtnDescriptor *p_doc) const;
  pxtnERR _io_UNIT_num_r(pxtnDescriptor *p_doc, int32_t *p_num);

  bool _x3x_TuningKeyEvent();
  bool _x3x_AddTuningEvent();
  bool _x3x_SetVoiceNames();

  //////////////
  // vomit..
  //////////////
  bool _moo_b_valid_data;
  bool _moo_b_end_vomit;
  bool _moo_b_init;

  mooParams _moo_params;
  mooState _moo_state;

  pxtnERR _init(int32_t fix_evels_num, bool b_edit);
  bool _release();
  pxtnERR _pre_count_event(pxtnDescriptor *p_doc, int32_t *p_count);

  void _moo_constructor();
  void _moo_destructer();
  bool _moo_init();
  bool _moo_release();

  bool _moo_ResetVoiceOn(pxtnUnit *p_u, int32_t w) const;
  bool _moo_InitUnitTone();
  void _moo_ProcessEvent(pxtnUnit *p_u, int32_t u, const EVERECORD *e,
                         int32_t clock);
  bool _moo_PXTONE_SAMPLE(void *p_data);

  pxtnSampledCallback _sampled_proc;
  void *_sampled_user;

 public:
  pxtnService();
  ~pxtnService();

  pxtnText *text;
  pxtnMaster *master;
  pxtnEvelist *evels;

  pxtnERR init();
  pxtnERR init_collage(int32_t fix_evels_num);
  bool clear();

  pxtnERR write(pxtnDescriptor *p_doc, bool bTune, uint16_t exe_ver);
  pxtnERR read(pxtnDescriptor *p_doc);

  bool AdjustMeasNum();

  int32_t get_last_error_id() const;

  pxtnERR tones_ready();
  bool tones_clear();

  int32_t Group_Num() const;

  // delay.
  int32_t Delay_Num() const;
  int32_t Delay_Max() const;
  bool Delay_Set(int32_t idx, DELAYUNIT unit, float freq, float rate,
                 int32_t group);
  bool Delay_Add(DELAYUNIT unit, float freq, float rate, int32_t group);
  bool Delay_Remove(int32_t idx);
  pxtnERR Delay_ReadyTone(int32_t idx);
  pxtnDelay *Delay_Get(int32_t idx);

  // over drive.
  int32_t OverDrive_Num() const;
  int32_t OverDrive_Max() const;
  bool OverDrive_Set(int32_t idx, float cut, float amp, int32_t group);
  bool OverDrive_Add(float cut, float amp, int32_t group);
  bool OverDrive_Remove(int32_t idx);
  bool OverDrive_ReadyTone(int32_t idx);
  pxtnOverDrive *OverDrive_Get(int32_t idx);

  // woice.
  int32_t Woice_Num() const;
  int32_t Woice_Max() const;
  const pxtnWoice *Woice_Get(int32_t idx) const;
  pxtnWoice *Woice_Get_variable(int32_t idx);

  pxtnERR Woice_read(int32_t idx, pxtnDescriptor *desc, pxtnWOICETYPE type);
  pxtnERR Woice_ReadyTone(int32_t idx);
  bool Woice_Remove(int32_t idx);
  bool Woice_Replace(int32_t old_place, int32_t new_place);

  // unit.
  int32_t Unit_Num() const;
  int32_t Unit_Max() const;
  const pxtnUnit *Unit_Get(int32_t idx) const;
  pxtnUnit *Unit_Get_variable(int32_t idx);

  bool Unit_Remove(int32_t idx);
  bool Unit_Replace(int32_t old_place, int32_t new_place);
  bool Unit_AddNew();
  bool Unit_SetOpratedAll(bool b);
  bool Unit_Solo(int32_t idx);

  // q
  bool set_destination_quality(int32_t ch_num, int32_t sps);
  bool get_destination_quality(int32_t *p_ch_num, int32_t *p_sps) const;
  bool get_byte_per_smp(int32_t *p_byte_per_smp) const;
  bool set_sampled_callback(pxtnSampledCallback proc, void *user);

  //////////////
  // Moo..
  //////////////

  int32_t moo_tone_sample(pxtnUnit *p_u, void *data, int32_t buf_size,
                          int32_t time_pan_index) const;

  bool moo_is_valid_data() const;
  bool moo_is_end_vomit() const;
  const mooParams *moo_params() const { return &_moo_params; }

  bool moo_set_mute_by_unit(bool b);
  bool moo_set_loop(bool b);
  bool moo_set_fade(int32_t fade, float sec);
  bool moo_set_master_volume(float v);

  int32_t moo_get_total_sample() const;

  int32_t moo_get_now_clock() const;
  int32_t moo_get_end_clock() const;
  int32_t moo_get_sampling_offset() const;
  int32_t moo_get_sampling_end() const;

  bool moo_preparation(const pxtnVOMITPREPARATION *p_build);

  bool Moo(void *p_buf, int32_t size, int32_t *filled_size = nullptr);
};

int32_t pxtnService_moo_CalcSampleNum(int32_t meas_num, int32_t beat_num,
                                      int32_t sps, float beat_tempo);

#endif
