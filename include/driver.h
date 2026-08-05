// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef asdf_driver_h
#define asdf_driver_h

extern int stick_deadzone_factor;

void driver_read_input(void);
void driver_apply_dt_input(void);
void driver_apply_lift_input(void);

void temp_spin_dt(int left_power, int right_power);
void temp_c_spin_motor(int motor, int power);

#define claw_width_delta 0.48

void save_claw_pos(void);

#endif
