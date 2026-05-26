// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_hardware_h
#define tp_hardware_h

#include <inttypes.h>

#include "api.h"
#include "common.h"

static lang_agn_atomic_bool button_a_state;
static lang_agn_atomic_bool button_b_state;
static lang_agn_atomic_bool button_x_state;
static lang_agn_atomic_bool button_y_state;
static lang_agn_atomic_bool button_up_state;
static lang_agn_atomic_bool button_down_state;
static lang_agn_atomic_bool button_left_state;
static lang_agn_atomic_bool button_right_state;

static lang_agn_atomic_bool bumper_l1_state;
static lang_agn_atomic_bool bumper_l2_state;
static lang_agn_atomic_bool bumper_r1_state;
static lang_agn_atomic_bool bumper_r2_state;

static lang_agn_atomic_int axis_left_x;
static lang_agn_atomic_int axis_left_y;
static lang_agn_atomic_int axis_right_x;
static lang_agn_atomic_int axis_right_y;

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

typedef struct hardware
{
        int8_t MOTOR_L1;
        int8_t MOTOR_L2;
        int8_t MOTOR_L3;

        int8_t MOTOR_R1;
        int8_t MOTOR_R2;
        int8_t MOTOR_R3;
        motor_group_wrapper MOTORGROUP_L;
        motor_group_wrapper MOTORGROUP_R;
} hardware;

static hardware robot_hardware;

void init_hardware();
motor_group_wrapper new_motor_group_wrapper(int8_t m1, int8_t m2, int8_t m3);
void set_motor_group_wrapper(motor_group_wrapper group, int speed);


#endif
