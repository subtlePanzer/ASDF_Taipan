// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "api.h"
#include "driver.h"
#include "hardware.h"

int stick_deadzone_factor = 1;

static bool isNoBumpersPressed() {
        return !bumper_l1_state &&
                !bumper_r1_state &&
                !bumper_l2_state &&
                !bumper_r2_state;
}

void driver_read_input() {
        do {
                atomic_store(&button_a_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_A));
                atomic_store(&button_b_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_B));
                atomic_store(&button_x_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_X));
                atomic_store(&button_y_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_Y));

                atomic_store(&button_up_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_UP));
                atomic_store(&button_down_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_DOWN));
                atomic_store(&button_left_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_LEFT));
                atomic_store(&button_right_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_RIGHT));

                atomic_store(&axis_left_x, controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_LEFT_X));
                atomic_store(&axis_left_y, controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_LEFT_Y));
                atomic_store(&axis_right_x, controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_RIGHT_X));
                atomic_store(&axis_right_y, controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_RIGHT_Y));

                atomic_store(&bumper_l1_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_L1));
                atomic_store(&bumper_l2_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_L2));
                atomic_store(&bumper_r1_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R1));
                atomic_store(&bumper_r2_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R2));

                delay(10); // could even be lower?
        } while (true);
}

void driver_apply_dt_input() {
        do {
                // Deadzones
                int axis_h = atomic_load(&axis_left_x);
                if (abs(axis_h) < stick_deadzone_factor) axis_h = 0;

                int axis_v = atomic_load(&axis_left_y);
                if (abs(axis_v) < stick_deadzone_factor) axis_v = 0;


                int dt_left = axis_v + axis_h;
                int dt_right = axis_v - axis_h;

                motor_move(motor_left_front, dt_left);
                motor_move(motor_left_mid, dt_left);
                motor_move(motor_left_rear, dt_left);

                motor_move(motor_right_front, dt_right);
                motor_move(motor_right_mid, dt_right);
                motor_move(motor_right_rear, dt_right);

                set_motor_group_wrapper(&robot_hardware.MOTORGROUP_L, dt_left);
                set_motor_group_wrapper(&robot_hardware.MOTORGROUP_R, dt_right);

                delay(9);
        } while (true);
}

void driver_apply_lift_input() {
        motor_set_brake_mode(motor_lift_a, E_MOTOR_BRAKE_BRAKE);
        do {
                int lift_force = (int)(lift_control_factor * -(float)atomic_load(&axis_right_y));

                if (adi_digital_read(lim_switch_lift) && lift_force <= 0)
                        lift_force = 0;

                if (motor_get_current_draw(motor_lift_a) > 2000) // current overdraw
                        lift_force = 0;

                if (lift_force == 0)
                        motor_brake(motor_lift_a);
                else
                        motor_move(motor_lift_a, lift_force);

                delay(11);
        } while (true);
}
