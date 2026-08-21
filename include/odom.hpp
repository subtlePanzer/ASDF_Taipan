#pragma once

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

        // void calc_position() override {
        //         double a = -para_wheel.get_position(); // in centidegrees
        //         double b = -lat_wheel.get_position();

        //         double da = a - last_a;
        //         double db = b - last_b;

        //         double a_rots = da / (360 * 100.0); // convert to revs
        //         double b_rots = db / (360 * 100.0);

        //         double a_travel = a_rots * tracking_wheel_circ;
        //         double b_travel = b_rots * tracking_wheel_circ;

        //         double theta = heading.load(); // global heading
        //         double theta_rads = theta * M_PI / 180.0;
        //         // theta = 0;

        //         double cx = a_travel * cos(theta_rads) - b_travel * sin(theta_rads);
        //         double cy = a_travel * sin(theta_rads) + b_travel * cos(theta_rads);

        //         x.fetch_add(cx);
        //         y.fetch_add(cy);

        //         last_a = a;
        //         last_b = b;
        // }

        void calc_position() override {
                double s = -lat_wheel.get_position();
                double r = -para_wheel.get_position();

                double ds = (s - last_s) * cD2rots * tracking_wheel_circ;
                double dr = (r - last_r) * cD2rots * tracking_wheel_circ;

                printf("ds = %f\n", ds);
                printf("dr = %f\n", dr);

                last_s = s;
                last_r = r;

                double theta = heading.load() * (M_PI / 180.0);
                double dtheta = theta - last_theta;

                printf("dt = %f\n", dtheta);

                last_theta = theta;

                // double ddlx;
                // double ddly;
                // if (abs(dtheta) <= 0) {
                //         ddlx = ds;
                //         ddly = dr;
                // } else {
                double ddlx = 2 * sin(dtheta / 2) * ((ds / dtheta) + offset_s);
                double ddly = 2 * sin(dtheta / 2) * ((dr / dtheta) + offset_r);
                // }

                printf("ddlx = %f\n", ddlx);
                printf("ddly = %f\n", ddly);

                double thetam = last_theta + (dtheta / 2);

                // rotate by -thetam
                double ddx = ddlx * cos(thetam) - ddly * sin(thetam);
                double ddy = ddlx * sin(thetam) + ddly * cos(thetam);

                printf("ddx = %f\n", ddx);
                printf("ddy = %f\n", ddy);

                x.store(x.load() + ddx);
                y.store(y.load() + ddy);

                printf("x = %f\n", x.load());
                printf("y = %f\n", y.load());
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
