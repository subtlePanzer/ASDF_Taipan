// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef asdf_driver_h
#define asdf_driver_h

extern int stick_deadzone_factor;

void driver_read_input();
void driver_apply_dt_input();
void driver_apply_lift_input();

void temp_spin_dt(int left_power, int right_power);
void temp_c_spin_motor(int motor, int power);

#endif 
