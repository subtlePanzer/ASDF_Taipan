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

protected:
        std::atomic<double> tx;
        std::atomic<double> ty;
        std::atomic<bool> is_finished;

        IPositionable* position_source;
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
                float dkp = 1.6;
                float hkp = 0.7;

                float l_power;
                float r_power;
                float last_dist = 99999;
                float h;
                float dist;

                do {
                        float error = get_mag(delta);


                        if (is_turn_phase) {
                                float turn_error = shd(heading.load(), target_deg);
                                float turn_power = clamp(tkp * turn_error, -127, 127);

                                // power is low enough that it doesn't matter
                                if (turn_power < 1.0) { // TUNE
                                        is_turn_phase = false;
                                }
                                l_power = turn_power;
                                r_power = -turn_power;

                        } else {
                                h = heading.load();
                                position = position_source->get_position();

                                delta.x = tx - position.x;
                                delta.y = ty - position.y;

                                dist = get_mag(delta);

                                float fpower = dkp * dist;
                                float dot = delta.x * cos(h) + delta.y * sin(h);

                                if (dot < 0) fpower = -dkp * dist;

                                float h_err = shd(h, target_deg);

                                float turn_correct = h_err * hkp;

                                l_power = fpower - turn_correct;
                                r_power = fpower + turn_correct;

                                last_dist = dist;
                        }


                        set_motor_group_wrapper(robot_hardware.MOTORGROUP_L, l_power);
                        set_motor_group_wrapper(robot_hardware.MOTORGROUP_R, r_power);
                } while (!is_finished);
        }
};

class pure_pursuit : public navigator {

};
