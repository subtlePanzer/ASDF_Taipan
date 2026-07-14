// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "api.h"
#include "dead_reckoning.h"
#include "hardware.h"
#include "math.h"

lang_agn_atomic_int curr_x;
lang_agn_atomic_int curr_y;

#define DT_WHEEL_CIRCUM M_PI * 2 * 3.25; // TODO: check

vec2 get_pos(void) {
        vec2 out;
        out.x = atomic_load(&curr_x);
        out.y = atomic_load(&curr_y);

        return out;
}

static void init_dead_reckoner(void) {
        atomic_store(&curr_x, 0);
        atomic_store(&curr_y, 0);

        motor_set_encoder_units(motor_left_front, E_MOTOR_ENCODER_ROTATIONS);
        motor_set_encoder_units(motor_left_mid, E_MOTOR_ENCODER_ROTATIONS);
        motor_set_encoder_units(motor_left_rear, E_MOTOR_ENCODER_ROTATIONS);

        motor_set_encoder_units(motor_right_front, E_MOTOR_ENCODER_ROTATIONS);
        motor_set_encoder_units(motor_right_mid, E_MOTOR_ENCODER_ROTATIONS);
        motor_set_encoder_units(motor_right_rear, E_MOTOR_ENCODER_ROTATIONS);
}

static float av_motor_pos(void) {
        float sum;
        for (int i = 0; i < DT_MOTOR_C; i++) {
                sum += motor_get_position(DT_MOTOR_PORTS[i]);
        }

        return sum / DT_MOTOR_C;
}

void position_tracker(void) {
        init_dead_reckoner();

        float cx, cy, dx, dy, d, last_d, theta;

        do {
                d = av_motor_pos();

                d *= DT_WHEEL_CIRCUM;

                theta = atomic_load(&heading);

                dx = d * asin(theta);
                dy = d * acos(theta);

                cx += dx;
                cy += dy;

                atomic_store(&curr_x, cx);
                atomic_store(&curr_y, cy);

                last_d = d;

                delay(4); // 250 Hz
        } while (true);
}
