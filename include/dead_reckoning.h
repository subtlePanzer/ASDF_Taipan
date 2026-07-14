// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_dead_reckoning_h
#define tp_dead_reckoning_h

#include "auton.h"
#include "common.h"
#include <stdatomic.h>

extern lang_agn_atomic_int curr_x;
extern lang_agn_atomic_int curr_y;

vec2 get_pos(void);

void position_tracker(void);

#endif
