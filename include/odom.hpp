// -----------------------------------------------------------------------------
// ASDF 2026 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef ODOM_H
#define ODOM_H

#include "sensor_system.hpp"

#include <inttypes.h>
#include <memory>

extern "C" {
#include "api.h"
#include "hardware.h"
};

// #define cD2rots (1.0 / 36000.0)
#define cD2rots 2.777778E-5


class tracking_wheel {
public:
        tracking_wheel(int8_t port, double radius_mm, double offset) 
        : offset(offset) { 
                wheel = std::make_unique<pros::Rotation>(port);
                wheel->set_position(0);

                circ = M_PI * radius_mm * radius_mm;
        }

        ~tracking_wheel() {};

        double get_delta() { // Returns mm
                double pos = (double)wheel->get_position(); // assuming rot is in cDeg
                double raw_delta = pos - last_p;
                last_p = pos;

                return raw_delta * cD2rots * circ; // mm
        }

private:
        std::unique_ptr<pros::Rotation> wheel;
        double last_p;
        double circ;
        double offset;
};

class straight_odom_2_wheel : public sensor_sys { // Make an arc_odom
public:
        straight_odom_2_wheel(tracking_wheel& lateral_wheel, tracking_wheel& parallel_wheel/*, int twc*/) /*, tracking_wheel_circ(twc) */ {
                x.store(0);
                y.store(0);
                lat_wheel = &lateral_wheel;
                para_wheel = &parallel_wheel;
        };

        void calc_position() override {
                double ds = lat_wheel->get_delta();
                double dr = para_wheel->get_delta();

                double h = heading.load();
                if (h == INFINITY)
                {
                        printf("Heading not valid.\n");
                        return;
                }

                double theta = h * (M_PI / 180.0);

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
        tracking_wheel* lat_wheel;
        tracking_wheel* para_wheel;

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
