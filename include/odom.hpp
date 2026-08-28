// -----------------------------------------------------------------------------
// ASDF 2026 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef ODOM_H
#define ODOM_H

#include "sensor_system.hpp"

#include <inttypes.h>

extern "C" {
#include "api.h"
#include "hardware.h"
};

// #define cD2rots (1.0 / 36000.0)
#define cD2rots 2.777778E-5

class straight_odom_2_wheel : public sensor_sys { // Make an arc_odom
public:
        straight_odom_2_wheel(int8_t lateral_wheel, int8_t parallel_wheel/*, int twc*/) : lat_wheel(lateral_wheel), para_wheel(parallel_wheel) /*, tracking_wheel_circ(twc) */ {
                x.store(0);
                y.store(0);
                para_wheel.set_position(0);
                lat_wheel.set_position(0);
        };

        void calc_position() override {
                double s = -lat_wheel.get_position();
                double r = -para_wheel.get_position();

                double ds = (s - last_s) * cD2rots * tracking_wheel_circ;
                double dr = (r - last_r) * cD2rots * tracking_wheel_circ;

                last_s = s;
                last_r = r;

                double theta = heading.load() * (M_PI / 180.0);
                double dtheta = theta - last_theta;

                last_theta = theta;

                double ddlx;
                double ddly;
                if (abs(dtheta) <= 0) {
                        ddlx = ds;
                        ddly = dr;
                } else {
                        ddlx = 2 * sin(dtheta / 2) * ((ds / (dtheta * (1 / RAD2DEG))) + offset_s);
                        ddly = 2 * sin(dtheta / 2) * ((dr / (dtheta * (1 / RAD2DEG))) + offset_r);
                }

                double thetam = last_theta + (dtheta / 2);

                // rotate by -thetam
                double ddx = ddlx * cos(thetam * (1 / RAD2DEG)) - ddly * sin(thetam * (1 / RAD2DEG));
                double ddy = ddlx * sin(thetam * (1 / RAD2DEG)) + ddly * cos(thetam * (1 / RAD2DEG));

                x.store(x.load() + ddx);
                y.store(y.load() + ddy);
        }

private:
        pros::Rotation lat_wheel;
        pros::Rotation para_wheel;

        double last_s = 0;
        double last_r = 0;
        double last_theta = 0;

        double start_s = 0;
        double start_r = 0;
        double start_theta = 0;

        double offset_s = -60.0;
        double offset_r = -10.0;

        double tracking_wheel_circ = 159.5;
};

#endif
