// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "hardware.h"

lang_agn_atomic_bool button_a_state;
lang_agn_atomic_bool button_b_state;
lang_agn_atomic_bool button_x_state;
lang_agn_atomic_bool button_y_state;
lang_agn_atomic_bool button_up_state;
lang_agn_atomic_bool button_down_state;
lang_agn_atomic_bool button_left_state;
lang_agn_atomic_bool button_right_state;

lang_agn_atomic_bool bumper_l1_state;
lang_agn_atomic_bool bumper_l2_state;
lang_agn_atomic_bool bumper_r1_state;
lang_agn_atomic_bool bumper_r2_state;

lang_agn_atomic_int axis_left_x;
lang_agn_atomic_int axis_left_y;
lang_agn_atomic_int axis_right_x;
lang_agn_atomic_int axis_right_y;

void init_hardware() {
        robot_hardware.MOTOR_L1 = (uint8_t)motor_left_front;
        robot_hardware.MOTOR_L2 = (uint8_t)motor_left_mid;
        robot_hardware.MOTOR_L3 = (uint8_t)motor_left_rear;
        robot_hardware.MOTOR_R1 = (uint8_t)motor_right_front;
        robot_hardware.MOTOR_R2 = (uint8_t)motor_right_mid;
        robot_hardware.MOTOR_R3 = (uint8_t)motor_right_rear;

        new_motor_group_wrapper(&robot_hardware.MOTORGROUP_L,
                robot_hardware.MOTOR_L1,
                robot_hardware.MOTOR_L2,
                robot_hardware.MOTOR_L3);

        new_motor_group_wrapper(&robot_hardware.MOTORGROUP_R,
                robot_hardware.MOTOR_R1,
                robot_hardware.MOTOR_R2,
                robot_hardware.MOTOR_R3);

        atomic_init(&button_a_state, false);
        atomic_init(&button_b_state, false);
        atomic_init(&button_x_state, false);
        atomic_init(&button_y_state, false);
        atomic_init(&button_up_state, false);
        atomic_init(&button_down_state, false);
        atomic_init(&button_left_state, false);
        atomic_init(&button_right_state, false);

        atomic_init(&axis_left_x, 0);
        atomic_init(&axis_right_x, 0);
        atomic_init(&axis_left_y, 0);
        atomic_init(&axis_right_y, 0);
}

motor_group_wrapper new_motor_group_wrapper(motor_group_wrapper* m, int8_t m1, int8_t m2, int8_t m3) {
        m->m1 = m1;
        m->m2 = m2;
        m->m3 = m3;
}

void set_motor_group_wrapper(motor_group_wrapper* group, int speed) { // Actually more memory efficient to pass the wrapper directly (3x8 = 24 bits vs 32/64 bits for a pointer)
        motor_move(group->m1, speed);
        motor_move(group->m2, speed);
        motor_move(group->m3, speed);
}
