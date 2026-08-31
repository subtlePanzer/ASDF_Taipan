// -----------------------------------------------------------------------------
// ASDF 2026 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef ODOM_H
#define ODOM_H

#include "sensor_system.hpp"
#include "units.hpp"

#include <inttypes.h>
#include <memory>

extern "C" {
#include "api.h"
#include "hardware.h"
};

// #define cD2rots (1.0 / 36000.0)
#define cD2rots 2.777778E-5


template<typename radius_unit = mm, typename offset_unit = mm>
class tracking_wheel {
public:
        tracking_wheel(int8_t port, radius_unit radius, offset_unit offset) 
        : offset(offset) { 
                wheel = std::make_unique<pros::Rotation>(port);
                wheel->set_position(0);

                circ = M_PI * radius * radius;
        }

        ~tracking_wheel() {};

        template<typename T = mm>
        T get_delta() {
                cdeg pos = wheel->get_position();
                cdeg raw_delta = pos - last_p;
                last_p = pos;

                return (T)(raw_delta * cD2rots * circ);
        }

        template<typename T = mm>
        T get_offset() {
                return offset;
        }

private:
        std::unique_ptr<pros::Rotation> wheel;
        cdeg last_p;
        mm circ;
        offset_unit offset;
};

class straight_odom_2_wheel : public sensor_sys { // Make an arc_odom
public:
        straight_odom_2_wheel(tracking_wheel& lateral_wheel, tracking_wheel& parallel_wheel) {
                x.store(0);
                y.store(0);

                lat_wheel = &lateral_wheel;
                para_wheel = &parallel_wheel;
        };

        void calc_position() override {
                mm ds = lat_wheel->get_delta();
                mm dr = para_wheel->get_delta();

                deg h = heading.load();
                if (h == INFINITY)
                {
                        printf("Heading not valid.\n");
                        return;
                }

                rad theta = (rad)h;

                rad dtheta = theta - last_theta;

                last_theta = theta;

                mm ddlx;
                mm ddly;
                if (abs(dtheta) <= 0) {
                        ddlx = ds;
                        ddly = dr;
                } else {
                        ddlx = 2 * std::sin(dtheta / 2) * ((ds / (dtheta)) + lat_wheel->get_offset<mm>());
                        ddly = 2 * std::sin(dtheta / 2) * ((dr / (dtheta)) + para_wheel->get_offset<mm>());
                }

                rad thetam = last_theta + (dtheta / 2);

                // rotate by -thetam to get to global position space
                mm ddx = ddlx * std::cos(thetam) - ddly * std::sin(thetam);
                mm ddy = ddlx * std::sin(thetam) + ddly * std::cos(thetam);

                x.store(x.load() + ddx);
                y.store(y.load() + ddy);
        }

private:
        tracking_wheel* lat_wheel;
        tracking_wheel* para_wheel;

        mm last_s = 0;
        mm last_r = 0;
        rad last_theta = 0;

        mm offset_s = -60.0;
        mm offset_r = -10.0;
};

#endif
