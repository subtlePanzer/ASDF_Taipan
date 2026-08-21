// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "api.h"
#include "driver.h"
#include "hardware.h"

#define stick_deadzone_factor 1 // TODO: Adjust

controller_status ct_status;

double claw_rest_pos = 0;
bool is_claw_open = false;

void save_claw_pos(void) {
        claw_rest_pos = motor_get_position(motor_claw);

        if (claw_rest_pos == PROS_ERR_F) {
                printf("Saving the claw position failed, aborting...\n");
                printf("Error: %s", errno);
                exit(1);
        } else
                printf("Claw rest pos: %d\n", claw_rest_pos);

        is_claw_open = false;
}

static bool isNoBumpersPressed(void) {
        return !ct_status.bumper_l1_state &&
                !ct_status.bumper_r1_state &&
                !ct_status.bumper_l2_state &&
                !ct_status.bumper_r2_state;
}

void temp_spin_dt(int left_power, int right_power) {
        set_motor_group_wrapper(robot_hardware.MOTORGROUP_L, left_power);
        set_motor_group_wrapper(robot_hardware.MOTORGROUP_L, right_power);

        // motor_move(motor_left_front, left_power);
        // motor_move(motor_left_mid, left_power);
        // motor_move(motor_left_rear, left_power);

        // motor_move(motor_right_front, right_power);
        // motor_move(motor_right_mid, right_power);
        // motor_move(motor_right_rear, right_power);
}

void temp_c_spin_motor(int motor, int power) {
        motor_move(motor, power);
}

void driver_read_input() {
        do {
                atomic_store(&ct_status.button_a_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_A));
                atomic_store(&ct_status.button_b_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_B));
                atomic_store(&ct_status.button_x_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_X));
                atomic_store(&ct_status.button_y_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_Y));

                atomic_store(&ct_status.button_up_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_UP));
                atomic_store(&ct_status.button_down_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_DOWN));
                atomic_store(&ct_status.button_left_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_LEFT));
                atomic_store(&ct_status.button_right_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_RIGHT));

                atomic_store(&ct_status.axis_left_x, controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_LEFT_X));
                atomic_store(&ct_status.axis_left_y, controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_LEFT_Y));
                atomic_store(&ct_status.axis_right_x, controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_RIGHT_X));
                atomic_store(&ct_status.axis_right_y, controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_RIGHT_Y));

                atomic_store(&ct_status.bumper_l1_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_L1));
                atomic_store(&ct_status.bumper_l2_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_L2));
                atomic_store(&ct_status.bumper_r1_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R1));
                atomic_store(&ct_status.bumper_r2_state, controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R2));

                delay(10); // could even be lower?
        } while (true);
}

static float clamp(float v, float minv, float maxv) {
        return fmax(v, fmin(v, maxv));
}

void driver_apply_dt_input() {
        double steering_coeff = 0.85;
        do {
                // Deadzones
                int axis_h = clamp(atomic_load(&ct_status.axis_right_x) * steering_coeff /* + atomic_load(&ct_status.axis_left_x) */, -127, 127);

                int axis_v = atomic_load(&ct_status.axis_left_y);

\
                if (abs(axis_h) < stick_deadzone_factor) axis_h = 0;
                if (abs(axis_v) < stick_deadzone_factor) axis_v = 0;

                int dt_left = axis_v + axis_h;
                int dt_right = axis_v - axis_h;


                motor_move(motor_left_front, dt_left);
                motor_move(motor_left_mid, dt_left);
                motor_move(motor_left_rear, dt_left);

                motor_move(motor_right_front, dt_right);
                motor_move(motor_right_mid, dt_right);
                motor_move(motor_right_rear, dt_right);

                // set_motor_group_wrapper(robot_hardware.MOTORGROUP_L, dt_left);
                // set_motor_group_wrapper(robot_hardware.MOTORGROUP_R, dt_right);

                delay(9);
        } while (true);
}

void driver_apply_lift_input() {
        motor_set_brake_mode(motor_lift_a, E_MOTOR_BRAKE_BRAKE);
        motor_set_brake_mode(motor_lift_b, E_MOTOR_BRAKE_BRAKE);
        motor_set_brake_mode(motor_claw, E_MOTOR_BRAKE_COAST);
        motor_set_encoder_units(motor_claw, E_MOTOR_ENCODER_ROTATIONS);
        adi_port_set_config(1, E_ADI_DIGITAL_IN);

        float lift_control_factor = 0.85;
        float lift_bias = -3.0;
        float lift_delay = 0.0;

        float claw_accuracy = 0.05;
        float claw_passive_grab_power = 20.0;
        bool claw_open = false;

        do {
                int lift_force = (int)(lift_control_factor * -(float)atomic_load(&ct_status.axis_right_y) + lift_bias);

                if (adi_digital_read(1) && lift_force <= 0)
                        lift_force = 0;

                // if (controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R1) && !is_claw_open)
                //         printf("Should open? %d\n", controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R1) && !is_claw_open);
                // if (controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R2) && is_claw_open)
                //         printf("Should close? %d\n", controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R2) && is_claw_open);

                // printf("Current: %d\n", motor_get_current_draw(motor_claw));

                // if (controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R1) && !is_claw_open) {
                //         motor_move_absolute(motor_claw, claw_rest_pos, 127.0);
                //         while (!((motor_get_position(motor_claw) < (claw_rest_pos + claw_accuracy)) && (motor_get_position(motor_claw) > (claw_rest_pos - claw_accuracy))))
                //                 delay(2); // This needs it's own thread because it's blocking

                //         is_claw_open = true;
                //         motor_move(motor_claw, claw_passive_grab_power);
                //         // motor_move(motor_claw, 0);
                // } else if (controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R2) && is_claw_open) {
                //         motor_move_absolute(motor_claw, claw_rest_pos + claw_width_delta, 127.0);

                //         while (!((motor_get_position(motor_claw) < (claw_rest_pos + claw_width_delta + claw_accuracy)) && (motor_get_position(motor_claw) > (claw_rest_pos + claw_width_delta - claw_accuracy))))
                //                 delay(2); // This needs it's own thread because it's blocking

                //         is_claw_open = false;
                //         motor_move(motor_claw, 0);
                // }

                if (controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R1))
                motor_move(motor_claw, 127.0);
        else if (controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_R2))
                motor_move(motor_claw, -127.0);
        else
                motor_move(motor_claw, 0.0);

                lift_delay -= (11.0 / 1000.0);

                // if (lift_force >= 0 && (motor_get_position(motor_lift_a) >= -LIFT_EPSILON))
                //         lift_force = 0;

                // if (motor_get_current_draw(motor_lift_a) > 2000) // Default current limit is 2500 before the motor shuts down, we can override that if needed
                // {
                //         lift_force = 0; // prevent twisting shafts again hopefully
                //         motor_set_zero_position(motor_lift_a, 0.1); // Check if maybe I want to protect the brain by reversing or something similar?
                // }

                if (lift_force == 0)
                {
                        motor_brake(motor_lift_a);
                        motor_brake(motor_lift_b);
                } else {
                        motor_move(motor_lift_a, lift_force);
                        motor_move(motor_lift_b, lift_force);
                }
                delay(11);
        } while (true);
}
