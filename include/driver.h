// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef asdf_driver_h
#define asdf_driver_h

extern int stick_deadzone_factor;

void driver_read_input();
void driver_apply_dt_input();
void driver_apply_lift_input();

#endif 
