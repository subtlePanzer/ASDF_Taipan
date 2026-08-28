#pragma once

#include <atomic>
#include "IPositionable.hpp"

extern "C" {
#include "auton.h"
#include "_math.h"
#include "hardware.h"
};

class navigator {
public:
        virtual ~navigator() = default;

        void set_target(vec2 target) {
                tx = target.x;
                ty = target.y;
        }

        virtual void navigate_thread() = 0;

        bool check_is_finished() const {
                return is_finished.load();
        }

        IPositionable* position_source;

protected:
        std::atomic<double> tx;
        std::atomic<double> ty;
        std::atomic<bool> is_finished;

};

class p2p : public navigator {
public:
        void navigate_thread() override {
                // do some setup

                bool is_turn_phase = true;

                vec2 position = position_source->get_position();
                double dx = tx - position.x;
                double dy = ty - position.y;

                vec2 delta = vec2(dx, dy);
                float target_deg = argd(delta);

                float tkp = 4.0;

                float dkp = .25;
                float dki = 0.00;
                float dkd = 0.00;
                float integral = 0;

                float hkp = 20.0;

                float l_power;
                float r_power;
                float last_dist = 99999;
                float h;
                float dist;

                do {
                        float error = get_mag(delta);


                        // if (is_turn_phase) {
                        //         float turn_error = shd(heading.load(), target_deg);
                        //         float turn_power = clamp(tkp * turn_error, -127, 127);

                        //         // power is low enough that it doesn't matter
                        //         if (turn_power < 1.0) { // TUNE
                        //                 is_turn_phase = false;
                        //         }
                        //         l_power = turn_power;
                        //         r_power = -turn_power;

                        // } else {
                        h = heading.load();
                        position = position_source->get_position();

                        delta.x = tx - position.y;
                        delta.y = ty - position.x;

                        dist = get_mag(delta);

                        target_deg = argd(delta);

                        float p = dkp * dist;

                        integral += dist;
                        integral = fmax(integral, 500);

                        if (p > 100) integral = 0;

                        float i = dki * integral;

                        float d = dkd * (dist - last_dist);

                        float fpower = p + i + d;
                        fpower = fmax(fpower, 127);


                        float dot = delta.x * cos(h) + delta.y * sin(h);

                        if (dot < 0) fpower = -dkp * dist;

                        float h_err = target_deg - h;

                        float turn_correct = h_err * hkp;

                        l_power = fpower + turn_correct;
                        r_power = fpower - turn_correct;

                        printf("HD: %.2f T: %.2f Dev: %.2f, Correct: %.2f\n", h, target_deg, target_deg - h, turn_correct);
                        printf("delta: %f %f dist: %.2f\n", delta.x, delta.y, dist);
                        printf("power: %.2f, %.2f\n", l_power, r_power);

                        last_dist = dist;
                        // }

                        set_motor_group_wrapper(robot_hardware.MOTORGROUP_L, l_power);
                        set_motor_group_wrapper(robot_hardware.MOTORGROUP_R, r_power);

                        if (dist < 5 && fpower < 1)
                                is_finished = true;

                        pros::delay(20);
                } while (!is_finished);
        }

        void set_fin() {
                is_finished = true;
        }
};

class pure_pursuit : public navigator {

};
