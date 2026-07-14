// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "auton.h"
#include "dead_reckoning.h"
#include "hardware.c"
#include <stdatomic.h>

#define RAD2DEG (M_PI / 180)

static float get_mag(vec2 v) {
        return pow(v.x * v.x + v.y * v.y, 0.5);
}

static float shd(float h1, float h2) { // signed heading difference
        float value = fmod(h2 - h1 + 180, 360);
        if (value < 0)
                value += 360;
        return value - 180;
}

static float argd(vec2 v) {
        return atan2(v.y, v.x) * RAD2DEG;
}

static float clamp(float v, float minv, float maxv) {
        return max(v, min(v, maxv));
}

void auton_point_to_point(vec2 target) {
        vec2 delta;
        delta.x = target.x - atomic_load(&curr_x);
        delta.y = target.y - atomic_load(&curr_y);

        float dist = get_mag(delta);
        float target_degrees = argd(delta);

        // Do a simple P (without I D) to point roughly at target
        float turn_kp = 4.0;
        float turn_power;
        do {
                float turn_error = shd(atomic_load(&heading) * RAD2DEG, target_degrees);
                turn_power = clamp(turn_kp * turn_error, -127, 127);

                int dt_left = turn_power;
                int dt_right = -turn_power;

                motor_move(motor_left_front, dt_left);
                motor_move(motor_left_mid, dt_left);
                motor_move(motor_left_rear, dt_left);

                motor_move(motor_right_front, dt_right);
                motor_move(motor_right_mid, dt_right);
                motor_move(motor_right_rear, dt_right);

                delay(10);
        } while (fabs(shd(atomic_load(&heading) * RAD2DEG, target_degrees)));

        // PID with heading lock
        float dkp = 1.6; // P to distance -> roughly speed
        float hkp = 0.7; // P to heading error -> heading lock gain | must be less than 0 or robot will not be able to drive straight

        float dist = 99999;
        float last_dist = 99999;
        do { // TODO: add timeout
                // update the delta
                delta.x = target.x - atomic_load(&curr_x);
                delta.y = target.y - atomic_load(&curr_y);

                dist = get_mag(delta);
                float h = atomic_load(&heading);

                float fpower = dkp * dist;
                float dot = delta.x * cos(h) + delta.y * sin(h);

                if (dot < 0) fpower = -dkp * dist;

                // Determine drift for heading lock
                float h_error = shd(h * RAD2DEG, target_degrees);

                float turn_correct = h_error * hkp;

                float dt_left = fpower - turn_correct;
                float dt_right = fpower + turn_correct;

                motor_move(motor_left_front, dt_left); // make this a fucking function (MACRO YOU DUMB MF)
                motor_move(motor_left_mid, dt_left);
                motor_move(motor_left_rear, dt_left);

                motor_move(motor_right_front, dt_right);
                motor_move(motor_right_mid, dt_right);
                motor_move(motor_right_rear, dt_right);

                last_dist = dist;
        } while (fabs(dist) > 10.0);
}

void auton_read_sensors() {
        do {
                atomic_store(&heading, imu_get_heading(imu_port));

                delay(10); // could even be lower?
        } while (true);
}
