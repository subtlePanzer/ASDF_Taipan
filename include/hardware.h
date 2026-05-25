// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_hardware_h
#define tp_hardware_h

#include "api.h"
#include "common.h"

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

typedef enum {
        motor_left_front = -13,
        motor_left_mid = 14,
        motor_left_rear = -15,

        motor_right_front = 16,
        motor_right_mid = -17,
        motor_right_rear = 18,
} standard_ports;

typedef struct
{
        int8_t m1;
        int8_t m2;
        int8_t m3;
} motor_group_wrapper;

motor_group_wrapper new_motor_group_wrapper(int8_t m1, int8_t m2, int8_t m3);

void set_motor_group_wrapper(motor_group_wrapper group, int speed);

struct
{
        struct
        {
                int8_t MOTOR_L1;
                int8_t MOTOR_L2;
                int8_t MOTOR_L3;

                int8_t MOTOR_R1;
                int8_t MOTOR_R2;
                int8_t MOTOR_R3;
        } motors;

        struct
        {
                motor_group_wrapper MOTORGROUP_L;
                motor_group_wrapper MOTORGROUP_R;
        } motorgroups;
} hardware;

void init_hardware();

static struct hardware robot_hardware;

#endif
