#include "EditState.h"

EditState::EditState(int quantize_clock, int quantize_pitch)
    : mouse_edit_state({MouseEditState::Type::Nothing, 0, 0, 0, 0}),
      scale(),
      m_current_unit(0),
      m_quantize_clock(quantize_clock),
      m_quantize_pitch(quantize_pitch) {}
