// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "auton.h"
#include "dead_reckoning.h"
#include "hardware.h"
#include "_math.h"
#include <stdatomic.h>

#define IN_RAD(deg) ((deg) * M_PI / 180.0f)

static int sgn(float v) {
        return (v > 0) - (v < 0);
}

void set_motors_brake() {
        motor_set_brake_mode(motor_left_front, E_MOTOR_BRAKE_BRAKE);
        motor_set_brake_mode(motor_left_rear, E_MOTOR_BRAKE_BRAKE);

        motor_set_brake_mode(motor_right_front, E_MOTOR_BRAKE_BRAKE);
        motor_set_brake_mode(motor_right_rear, E_MOTOR_BRAKE_BRAKE);

        motor_set_brake_mode(motor_lift_a, E_MOTOR_BRAKE_BRAKE);
        motor_set_brake_mode(motor_lift_b, E_MOTOR_BRAKE_BRAKE);
}

void unset_motors_brake() {
        motor_set_brake_mode(motor_left_front, E_MOTOR_BRAKE_COAST);
        motor_set_brake_mode(motor_left_rear, E_MOTOR_BRAKE_COAST);

        motor_set_brake_mode(motor_right_front, E_MOTOR_BRAKE_COAST);
        motor_set_brake_mode(motor_right_rear, E_MOTOR_BRAKE_COAST);
}

#define SPIN_DT_MOTORS(power_l, power_r) \
        do { \
                motor_move(motor_left_front, (power_l)); \
                motor_move(motor_left_rear, (power_l)); \
        \
                motor_move(motor_right_front, (power_r)); \
                motor_move(motor_right_rear, (power_r)); \
        } while (0)

#define BRAKE_DT_MOTORS() \
        do { \
                motor_brake(motor_left_front); \
                motor_brake(motor_left_rear); \
        \
                motor_brake(motor_right_front); \
                motor_brake(motor_right_rear); \
        } while (0)

inline static bool is_time_expired(float start_time, float amount) {
        if (millis() - start_time >= amount) return true;
        return false;
}

void initialise_pid_data(struct pid_data* pid) {
        // human settable
        pid->kp = 0;
        pid->ki = 0;
        pid->kd = 0;
        pid->anti_windup_range = 0;

        // internal
        pid->integral = 0;
        pid->prev_err = 0;
        pid->prev_time = -1;
}

#define TO_SEC(millis) ((millis) / 1000.0f)

static float pid_data_update(struct pid_data* pid, float err) {
        const float now = millis();
        float dt = (pid->prev_time == -1) ? 0 : now - pid->prev_time;
        pid->prev_time = now;

        const float prop = pid->kp * err;
        const float deriv = pid->kd * ((dt != 0) ? (err - pid->prev_err) / TO_SEC(dt) : 0);

        pid->integral += err * dt;
        if (sgn(err) != sgn(pid->prev_err)) pid->integral = 0;
        if (fabs(err) > pid->anti_windup_range && pid->anti_windup_range != 0) pid->integral = 0;

        const float integ = pid->ki * pid->integral;

        pid->prev_err = err;

        return prop + integ + deriv;
}

static float get_angle_err(float t_angle, float actual_angle, enum turn_dir dir) {
        while (t_angle < 0) t_angle += 360;
        while (t_angle > 360) t_angle -= 360;

        float error = t_angle - actual_angle;
        if (dir == TURN_NONE) return remainderf(error, 360);

        if (dir == TURN_CCW) return error < 0 ? error + 360 : error;
        else return error > 0 ? error - 360 : error;
}

static bool within_range_for_time(float input, struct within_range_data* data) {
        if (data->start_time == -1) data->start_time = millis();

        const float curr_time = millis();
        if (fabs(input) >= data->range) data->start_time = -1;
        else if (curr_time >= data->start_time + data->time) data->is_done = true;

        return data->is_done;
}

void initialise_within_range_data(struct within_range_data* data) {
        data->start_time = -1; // sentinel value
        data->time = -1;
        data->range = -1;

        data->is_done = false;
}

enum prev_side {
        PREVSIDE_NONE = -1,
        PREVSIDE_DIFFERENT = 0,
        PREVSIDE_SAME = 1
};

enum slew_direction {
        SLEW_DECREASING = -1,
        SLEW_NONE = 0,
        SLEW_INCREASING = 1
};

static float directional_slew(float target, float curr, float max_delta, float dt, enum slew_direction dir) {
        if (max_delta == 0) return target;

        const float delta = target - curr;

        if (dir == SLEW_INCREASING && delta < 0) return target;
        if (dir == SLEW_DECREASING && delta > 0) return target;

        if (fabs(delta) > fabs(max_delta * dt)) return curr + (max_delta * dt * sgn(delta));

        return target;
}

static float slew(float target, float curr, float max_delta, float dt) {
        return directional_slew(target, curr, max_delta, dt, SLEW_NONE);
}

struct out_data
{
        float err;
        float* prev_out;
        struct pid_data* pid;
        float slew_amt;
        float max_speed;
        float min_speed;
        bool is_reversed;
        bool is_close;

        float dt;
};

static float p2p_get_lat_out(struct out_data params) {
        float out = pid_data_update(params.pid, params.err);
        out = clamp(out, -params.max_speed, params.max_speed);

        out = params.is_close ? out : slew(out, *params.prev_out, params.slew_amt, params.dt);

        if (!params.is_close && params.is_reversed) out = clamp(out, -params.max_speed, -params.min_speed);
        else if (!params.is_close && !params.is_reversed) out = clamp(out, params.min_speed, params.max_speed);

        *params.prev_out = out;
        return out;
}

static float p2p_get_ang_out(struct out_data params) {
        if (params.is_close && fabsf(params.err) > 40.0f) return 0;

        float out = pid_data_update(params.pid, params.err);

        out = clamp(out, -params.max_speed, params.max_speed);

        out = slew(out, *params.prev_out, params.slew_amt, params.dt);

        *params.prev_out = out;
        return out;
}

// Takes the delta to the target and turns it into the angle to the target; Probably needs a clearer name
#define ANGLE_TO(delta) (argd((delta)) - 90.0f)

static vec2 desaturate_power(float lat, float ang) {
        vec2 out;
        out.x = lat - ang;
        out.y = lat + ang;

        const float sum = fmax(fabs(out.x), fabs(out.y));
        if (sum <= 1.0) return out;

        out.x /= sum;
        out.y /= sum;
        return out;
}

static float calc_angle_err(struct auton_turn_target target, const enum turn_dir dir) {
        if (target.is_position) return ANGLE_TO(target.as.position);
        else return get_angle_err(target.as.heading, atomic_load(&heading), dir);
}

static enum slew_direction get_slew_dir(struct auton_turn_target target) {
        if (target.dir == TURN_CCW) return SLEW_INCREASING;
        if (target.dir == TURN_CW) return SLEW_DECREASING;

        float error = calc_angle_err(target, TURN_NONE);
        if (error > 0) return SLEW_INCREASING;
        else return SLEW_DECREASING;
}

struct motor_brake_set {
        motor_brake_mode_e_t lf;
        motor_brake_mode_e_t lr;

        motor_brake_mode_e_t rf;
        motor_brake_mode_e_t rr;
};

// TODO: Check drive motor ports

static void get_motor_brake_status(struct motor_brake_set* brake_status) {
        brake_status->lf = motor_get_brake_mode(motor_left_front);
        brake_status->lr = motor_get_brake_mode(motor_left_rear);

        brake_status->rf = motor_get_brake_mode(motor_right_front);
        brake_status->rr = motor_get_brake_mode(motor_right_rear);
}

static void set_motor_brake_status(struct motor_brake_set brake_status) {
        motor_set_brake_mode(motor_left_front, brake_status.lf);
        motor_set_brake_mode(motor_left_rear, brake_status.lr);

        motor_set_brake_mode(motor_right_front, brake_status.rf);
        motor_set_brake_mode(motor_right_rear, brake_status.rr);
};

static float get_dtheta(const struct auton_turn_target target, bool* is_settling, float* prev_raw_dtheta, float* prev_dtheta, bool* is_prev_raw_dtheta, bool* is_prev_dtheta, const enum turn_dir dir) {
        const float raw = calc_angle_err(target, TURN_NONE);
        *is_settling = is_prev_raw_dtheta && (sgn(raw) != sgn(*prev_raw_dtheta));
        *prev_raw_dtheta = raw;
        *is_prev_raw_dtheta = true;
        const float error = calc_angle_err(target, *is_settling ? TURN_NONE : dir);
        if (!is_prev_dtheta) {
                *prev_dtheta = error;
                *is_prev_dtheta = true;
        }
        return error;
}

static float constrain_power(float power, float min, float max) {
        if (fabs(power) < min) power = sgn(power) * min;
        power = clamp(power, -max, max);
        return power;

}

static float get_power(float dtheta, bool is_settling, float prev_motor_power, struct turn_params params, enum slew_direction slew_dir) {
        float raw = pid_data_update(params.turn_pid, dtheta);
        if (!is_settling) raw = directional_slew(raw, prev_motor_power, params.slew, params.dt, slew_dir);
        return constrain_power(raw, params.min_speed, params.max_speed);
}

// TODO: Make everything const in all these funcs
void improved_auton_turn(struct auton_turn_target target, struct auton_command_data cmd, struct turn_params params) {
        if (params.turn_exit_range_data != NULL) {
                params.turn_exit_range_data->start_time = -1;
                params.turn_exit_range_data->is_done = false;
        }

        if (params.turn_pid != NULL) {
                params.turn_pid->integral = 0;
                params.turn_pid->prev_err = 0;
                params.turn_pid->prev_time = -1;
        }
        const enum slew_direction slew_dir = get_slew_dir(target);

        float prev_raw_dtheta = 0;
        float prev_dtheta = 0;
        bool has_prev_raw_dtheta = false;
        bool has_prev_dtheta = false;

        float dtheta = INFINITY;
        bool is_settling = false;
        float prev_motor_power = 0;

        // Save the brake modes of the motors for later restoration
        struct motor_brake_set brake_status;
        get_motor_brake_status(&brake_status);
        if (target.lock_side) {
                if (target.lock_side == LOCKED_LEFT) {
                        motor_set_brake_mode(motor_left_front, E_MOTOR_BRAKE_BRAKE);
                        motor_set_brake_mode(motor_left_rear, E_MOTOR_BRAKE_BRAKE);
                } else {
                        motor_set_brake_mode(motor_right_front, E_MOTOR_BRAKE_BRAKE);
                        motor_set_brake_mode(motor_right_rear, E_MOTOR_BRAKE_BRAKE);
                }
        }

        const float start_millis = millis();

        while (!is_time_expired(start_millis, cmd.timeout_millis) && !within_range_for_time(dtheta, params.turn_exit_range_data)) {
                dtheta = get_dtheta(target, &is_settling, &prev_raw_dtheta, &prev_dtheta, &has_prev_raw_dtheta, &has_prev_dtheta, target.dir);

                if (params.min_speed != 0)
                        if (fabs(dtheta) < params.exit_range) break;
                        else if (has_prev_dtheta && (dtheta) != sgn(prev_dtheta)) break;

                prev_dtheta = dtheta;
                has_prev_dtheta = true;

                const float motor_power = get_power(dtheta, is_settling, prev_motor_power, params, slew_dir);

                prev_motor_power = motor_power;

                screen_print(E_TEXT_MEDIUM, 3, "Moving with {%.3f} dtheta", dtheta);
                screen_print(E_TEXT_MEDIUM, 4, "Power: {%.3f}, {%.3f}", -motor_power, motor_power);

                SPIN_DT_MOTORS(127.0f * -motor_power, 127.0f * motor_power);
                if (target.lock_side) {
                        if (target.lock_side == LOCKED_LEFT) {
                                motor_brake(dt_left_front);
                                motor_brake(dt_left_rear);
                        } else {
                                motor_brake(dt_right_front);
                                motor_brake(dt_right_rear);
                        }
                }

                delay(params.dt);
        }

        set_motor_brake_status(brake_status);
        BRAKE_DT_MOTORS();
}

void improved_point_to_point(vec2 target_pos, struct auton_command_data cmd, struct p2p_params params) {
        if (params.lat_exit_range_data != NULL) {
                params.lat_exit_range_data->start_time = -1;
                params.lat_exit_range_data->is_done = false;
        }

        if (params.lat_pid != NULL) {
                params.lat_pid->integral = 0;
                params.lat_pid->prev_err = 0;
                params.lat_pid->prev_time = -1;
        }

        if (params.ang_pid != NULL) {
                params.ang_pid->integral = 0;
                params.ang_pid->prev_err = 0;
                params.ang_pid->prev_time = -1;
        }

        vec2 initial_delta;
        initial_delta.x = target_pos.x - atomic_load(&curr_x);
        initial_delta.y = target_pos.y - atomic_load(&curr_y);

        const float initial_angle = ANGLE_TO(initial_delta);
        const float start_millis = millis();

        float prev_lat_out = 0.0f;
        float prev_ang_out = 0.0f;

        enum prev_side previous_side = PREVSIDE_NONE;

        vec2 pos, delta;

        bool is_close = false;

        while (!is_time_expired(start_millis, cmd.timeout_millis)) {
                // Not totally threadsafe since y may be updated after x has been read
                pos.x = atomic_load(&curr_x);
                pos.y = atomic_load(&curr_y);

                delta.x = target_pos.x - pos.x;
                delta.y = target_pos.y - pos.y;

                const float dist_to_target = get_mag(delta);

                if (!is_close && dist_to_target < 10.0f) {
                        is_close = true;

                        params.max_lat_speed = fmax(fabs(prev_lat_out), .2f);
                        params.max_ang_speed = fmax(fabs(prev_ang_out), .2f);
                }

                float h = atomic_load(&heading);

                const float lat_err = dist_to_target * cos(IN_RAD(get_angle_err(h, ANGLE_TO(delta), TURN_NONE)));
                const float ang_err = get_angle_err(cmd.reverse ? h + 180.0f : h, ANGLE_TO(delta), TURN_NONE);

                if (within_range_for_time(lat_err, params.lat_exit_range_data) && is_close) break;

                const bool side = (pos.x - target_pos.x) * -sin(IN_RAD(initial_angle)) + (pos.y - target_pos.y) * cos(IN_RAD(initial_angle)) <= -params.exit_range;
                if (previous_side == PREVSIDE_NONE) previous_side = (int)side;
                const bool is_same_side = (side == (int)previous_side);

                if (!is_same_side && params.min_lat_speed != 0) break;
                previous_side = (int)side;

                struct out_data lat_out_params;
                lat_out_params.err = lat_err;
                lat_out_params.prev_out = &prev_lat_out;
                lat_out_params.pid = params.lat_pid;
                lat_out_params.slew_amt = params.lat_slew;
                lat_out_params.max_speed = params.max_lat_speed;
                lat_out_params.min_speed = params.min_lat_speed;
                lat_out_params.is_reversed = cmd.reverse;
                lat_out_params.is_close = is_close;
                lat_out_params.dt = params.dt;

                struct out_data ang_out_params;
                ang_out_params.err = ang_err;
                ang_out_params.prev_out = &prev_ang_out;
                ang_out_params.pid = params.ang_pid;
                ang_out_params.slew_amt = params.ang_slew;
                ang_out_params.max_speed = params.max_ang_speed;
                ang_out_params.min_speed = params.min_ang_speed;
                ang_out_params.is_reversed = cmd.reverse;
                ang_out_params.is_close = is_close;
                ang_out_params.dt = params.dt;

                float lat_out = p2p_get_lat_out(lat_out_params);
                const float ang_out = p2p_get_ang_out(ang_out_params);

                if (is_close)
                        lat_out *= cosf(IN_RAD(ang_err));

                const vec2 out = desaturate_power(lat_out, ang_out);

                screen_print(E_TEXT_MEDIUM, 3, "Moving with {%.3f} lat, {%.3f} ang", lat_out, ang_out);
                screen_print(E_TEXT_MEDIUM, 4, "...{%.3f} lat err, {%.3f} ang err, {%.3f} dt", lat_err, ang_err, params.dt);
                screen_print(E_TEXT_MEDIUM, 5, "Power: {%.3f}, {%.3f}", out.x, out.y);

                SPIN_DT_MOTORS(127.0f * out.x, 127.0f * out.y);

                delay(params.dt);
        }

        BRAKE_DT_MOTORS();
}

static vec2 cartesian_from_polar(float mag, float argd) {
        vec2 out;
        out.x = mag * cos(IN_RAD(argd));
        out.y = mag * sin(IN_RAD(argd));
        return out;
}

static vec2 get_carrot_point(bool is_close, vec2 target, float heading, float lead, float dist) {
        if (is_close) return target;
        vec2 out;
        vec2 lead_point = cartesian_from_polar(lead * dist, heading);

        out.x = target.x - lead_point.x;
        out.y = target.y - lead_point.y;

        return out;
}

static struct auton_turn_target mutate_heading_pos_to_true_heading(vec2 ref_pos, struct auton_turn_target target_point) {
        vec2 delta; // extract to method to reduce reused code
        delta.x = target_point.as.position.x - ref_pos.x;
        delta.y = target_point.as.position.y - ref_pos.y;

        target_point.as.heading = ANGLE_TO(delta);
        target_point.is_position = false;

        return target_point;
}

static float get_p2p_turn_lat_err(float dist, float heading, float angle, bool is_close) {
        const float factor = cos(IN_RAD(get_angle_err(heading, angle, TURN_NONE)));
        if (is_close) dist *= factor; // dist is a local closure on the argument passed, it's safe to mutate
        else dist *= sgn(factor);
        return dist;
}

static float get_p2p_turn_ang_err(bool is_reversed, float heading, bool is_close, float target_heading, float angle_to_carrot) {
        const float adj_theta = is_reversed ? heading + 180 : heading;
        if (is_close) return get_angle_err(adj_theta, target_heading, TURN_NONE);
        else          return get_angle_err(adj_theta, angle_to_carrot, TURN_NONE);
}

static float p2p_turn_get_ang_out(float ang_err, struct p2p_turn_params params, float* prev_ang_out) {
        float out = pid_data_update(params.ang_pid, ang_err);
        out = clamp(out, -params.max_ang_speed, params.max_ang_speed);
        *prev_ang_out = out;
        return out;
}

static inline float get_signed_tan_arc_curve(vec2 start, float start_head, vec2 end) {
        vec2 delta;
        delta.x = end.x - start.x;
        delta.y = end.y - start.y;

        const float side = sgn(sin(IN_RAD(start_head)) * delta.x - cos(IN_RAD(start_head)) * delta.y);

        const float a = -tan(IN_RAD(start_head));
        const float c = tan(IN_RAD(start_head)) * start.x - start.y;
        const float x = fabs(a * end.x + end.y + c) / sqrt(a * a + 1);
        const float d = get_mag(delta);

        return side * ((2.0f * x) / (d * d));
}

// TODO: this 100% needs a parameter object
// 🤮
static float p2p_turn_get_lat_out(float lat_err, const float ang_out, struct p2p_turn_params params, bool is_close, float* prev_lat_out, vec2 pos, vec2 carrot_point, bool do_reverse) {
        float out = pid_data_update(params.lat_pid, lat_err);
        out = clamp(out, -params.max_lat_speed, params.max_lat_speed);
        if (!is_close) out = slew(out, *prev_lat_out, params.lat_slew, params.dt);

        // antislip
        const float radi = 1 / fabs(get_signed_tan_arc_curve(pos, atomic_load(&heading), carrot_point));
        const float max_slip_speed = sqrt(params.drift_compensation * radi / 1000.0f);
        out = clamp(out, -max_slip_speed, max_slip_speed);

        const float overturn = fabs(ang_out) + fabs(out) - params.max_lat_speed;
        if (overturn > 0) out -= (out > 0) ? overturn : -overturn;

        if (!is_close)
                out = do_reverse ? fmin(out, 0) : fmax(out, 0);

        if (do_reverse && -out < fabs(params.min_lat_speed) && out < 0) out = -fabs(params.min_lat_speed);
        if (!do_reverse && out < fabs(params.min_lat_speed) && out > 0) out = fabs(params.min_lat_speed);

        *prev_lat_out = out;
        return out;
}

void improved_pint_to_point_plus_heading(vec2 target_pos, struct auton_turn_target target_heading, struct auton_command_data cmd, struct p2p_turn_params params) {
        // TODO: reset structs
        if (params.lat_exit_range_data != NULL) {
                params.lat_exit_range_data->start_time = -1;
                params.lat_exit_range_data->is_done = false;
        }

        if (params.ang_exit_range_data != NULL) {
                params.ang_exit_range_data->start_time = -1;
                params.ang_exit_range_data->is_done = false;
        }

        if (params.lat_pid != NULL) {
                params.lat_pid->integral = 0;
                params.lat_pid->prev_err = 0;
                params.lat_pid->prev_time = -1;
        }

        if (params.ang_pid != NULL) {
                params.ang_pid->integral = 0;
                params.ang_pid->prev_err = 0;
                params.ang_pid->prev_time = -1;
        }

        vec2 last_pos;
        last_pos.x = atomic_load(&curr_x);
        last_pos.y = atomic_load(&curr_y);

        bool is_close = false;
        bool is_prev_same_side = false;

        float prev_lat_out = 0;
        float prev_ang_out = 0;

        float start_millis = millis();

        while (!is_time_expired(start_millis, cmd.timeout_millis)) {
                vec2 pos;
                pos.x = atomic_load(&curr_x);
                pos.y = atomic_load(&curr_y);

                vec2 delta;
                delta.x = target_pos.x - pos.x;
                delta.y = target_pos.y - pos.y;
                float dist = get_mag(delta);

                float heading = atomic_load(&heading);

                if (dist < 20.0f && !is_close) {
                        is_close = true;
                        params.max_lat_speed = fmax(fabs(prev_lat_out), 0.47);
                }

                // convert a target point to a heading
                if (target_heading.is_position)
                        target_heading = mutate_heading_pos_to_true_heading(target_pos, target_heading);

                const vec2 carrot_point = get_carrot_point(is_close, target_pos, target_heading.as.heading, params.lead, dist);

                vec2 dcarrot; // this really needs to be a function
                dcarrot.x = carrot_point.x - pos.x;
                dcarrot.y = carrot_point.y - pos.y;

                const float lat_error = get_p2p_turn_lat_err(dist, heading, ANGLE_TO(dcarrot), is_close);
                const float ang_error = get_p2p_turn_ang_err(cmd.reverse, heading, is_close, target_heading.as.heading, ANGLE_TO(dcarrot));

                if (within_range_for_time(lat_error, params.lat_exit_range_data)
                        && within_range_for_time(ang_error, params.ang_exit_range_data)
                        && is_close) {
                        break;
                }

                // Would be marginally clearer with an enum instead of a bool, however it's one case so I dunno
                const bool robot_side = (pos.y - target_pos.y) * -sin(IN_RAD(target_heading.as.heading)) <= (pos.x - target_pos.x) * cos(IN_RAD(target_heading.as.heading)) + params.exit_range;
                const bool carrot_side = (carrot_point.y - target_pos.y) * -sin(IN_RAD(target_heading.as.heading)) <= (carrot_point.x - target_pos.x) * cos(IN_RAD(target_heading.as.heading)) + params.exit_range;

                const bool is_same_side = robot_side == carrot_side;

                if (!is_same_side && is_prev_same_side && is_close && params.min_lat_speed != 0) break;

                is_prev_same_side = is_same_side;

                const float ang_out = p2p_turn_get_ang_out(ang_error, params, &prev_ang_out);
                const float lat_out = p2p_turn_get_lat_out(lat_error, ang_out, params, is_close, &prev_lat_out, pos, carrot_point, cmd.reverse);

                const vec2 out = desaturate_power(lat_out, ang_out);

                SPIN_DT_MOTORS(127.0f * out.x, 127.0f * out.y);

                delay(params.dt);
        }

        BRAKE_DT_MOTORS();

}
