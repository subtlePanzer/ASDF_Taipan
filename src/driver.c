// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

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
                button_a_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_A);
                button_b_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_B);
                button_x_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_X);
                button_y_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_Y);

                button_up_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_UP);
                button_down_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_DOWN);
                button_left_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_LEFT);
                button_right_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_RIGHT);

                axis_left_x = (lang_agn_atomic_bool)controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_LEFT_X);
                axis_left_y = (lang_agn_atomic_bool)controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_LEFT_Y);
                axis_right_x = (lang_agn_atomic_bool)controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_RIGHT_X);
                axis_right_y = (lang_agn_atomic_bool)controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_RIGHT_Y);

                bumper_l1_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_L1);
                bumper_l2_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_L2);
                bumper_r1_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R1);
                bumper_r2_state = (lang_agn_atomic_bool)controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R2);

                delay(10); // could even be lower?
        } while (true);
}

void driver_apply_input() {
        do {
                set_motor_group_wrapper(robot_hardware.MOTORGROUP_L,
                        axis_left_y + axis_right_x);
                set_motor_group_wrapper(robot_hardware.MOTORGROUP_R,
                        axis_left_y - axis_right_x);

                delay(9);
        } while (true);
}
