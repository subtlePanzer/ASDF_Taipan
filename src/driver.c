// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "api.h"
#include "driver.h"
#include "hardware.h"


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
                int i = atomic_load(&axis_left_y) + atomic_load(&axis_left_x);
                // motor_move(motor_left_front, i);
                // motor_move(motor_left_mid, i);
                // motor_move(motor_left_rear, i);

                int k = atomic_load(&axis_left_y) - atomic_load(&axis_left_x);
                // motor_move(motor_right_front, k);
                // motor_move(motor_right_mid, k);
                // motor_move(motor_right_rear, k);

                set_motor_group_wrapper(&robot_hardware.MOTORGROUP_L, i);
                set_motor_group_wrapper(&robot_hardware.MOTORGROUP_R, k);

                delay(9);
        } while (true);
}

void driver_apply_lift_input() {
        do {
                int lift_force = (int)(lift_control_factor * (float)atomic_load(&axis_right_x));

                motor_move(motor_lift_a, lift_force);

                delay(11);
        } while (true);
}
