// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_hardware_h
#define tp_hardware_h

#include <inttypes.h>

#include "api.h"
#include "common.h"

typedef struct {
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
} controller_status;

extern lang_agn_atomic_int heading;

// extern float lift_control_factor;
// extern float lift_bias;

typedef enum {
        dt_left_front,
        dt_left_mid,
        dt_left_rear,

        dt_right_front,
        dt_right_mid,
        dt_right_rear,

        DT_MOTOR_C // auto determined
} dt_motors;

typedef enum {
        motor_left_front = -11,
        motor_left_mid = 12,
        motor_left_rear = -13,

        motor_right_front = 20,
        motor_right_mid = -19,
        motor_right_rear = 18,

        motor_lift_a = 1, // right
        motor_lift_b = -10,
        motor_claw = 8,

        imu_port = 7,
        odom_para = 17,
        odom_perp = 9,
} standard_ports;

static const int8_t DT_MOTOR_PORTS[DT_MOTOR_C] = {
        [dt_left_front] = motor_left_front,
        [dt_left_mid] = motor_left_mid,
        [dt_left_rear] = motor_left_rear,

        [dt_right_front] = motor_right_front,
        [dt_right_mid] = motor_right_mid,
        [dt_right_rear] = motor_right_rear
};

typedef enum {
        lim_switch_lift = 1,
} adi_ports;

typedef struct
{
        int8_t m1;
        int8_t m2;
        int8_t m3;
} motor_group_wrapper;

typedef struct {
        int mcount;
        int8_t* motors;
} motor_group;

typedef struct hardware
{
        motor_group_wrapper MOTORGROUP_L;
        motor_group_wrapper MOTORGROUP_R;
} hardware;

#define LIFT_EPSILON 0.1 // The distance it will stop from the brain, roughly

extern hardware robot_hardware;

void init_hardware();
motor_group_wrapper new_motor_group_wrapper(motor_group_wrapper* m, int8_t m1, int8_t m2, int8_t m3);
void set_motor_group_wrapper(motor_group_wrapper group, int speed);

void calibrate_imu(void);

#endif
