#ifndef pxtnPulse_Frequency_H
#define pxtnPulse_Frequency_H

#include "./pxtn.h"

namespace pxtnPulse_Frequency {
float Get(int32_t key);
float Get2(int32_t key);
const float* GetDirect(int32_t* p_size);
};  // namespace pxtnPulse_Frequency

#endif
