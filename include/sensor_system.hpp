#pragma once

#include "auton.h"
#include <atomic>
#include "IPositionable.hpp"

class sensor_sys : public IPositionable {
public:
        virtual ~sensor_sys() = default;

        virtual void calc_position() = 0;

        vec2 get_position() override {
                return vec2(x.load(), y.load());
        }

protected:
        // Location stored in mm relative to the start position
        std::atomic<double> x;
        std::atomic<double> y;
};
