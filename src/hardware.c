// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "hardware.h"

void init_hardware() {
        hardware.motorgroups.MOTORGROUP_L = new_motor_group_wrapper(&hardware.motors.MOTOR_L1,
                &hardware.motors.MOTOR_L2,
                &hardware.motors.MOTOR_L3);
        hardware.motorgroups.MOTORGROUP_R = new_motor_group_wrapper(&hardware.motors.MOTOR_R1,
                &hardware.motors.MOTOR_R2,
                &hardware.motors.MOTOR_R3);
}

motor_group_wrapper new_motor_group_wrapper(int8_t m1, int8_t m2, int8_t m3) {
        motor_group_wrapper m;
        m.m1 = m1;
        m.m2 = m2;
        m.m3 = m3;
        return m;
}

set_motor_group_wrapper(motor_group_wrapper group, int speed) { // Actually more memory efficient to pass the wrapper directly (3x8 = 24 bits vs 32/64 bits for a pointer)
        motor_move(group.m1, speed);
        motor_move(group.m2, speed);
        motor_move(group.m3, speed);
}
