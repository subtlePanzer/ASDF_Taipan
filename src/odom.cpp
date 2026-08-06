#include "sensor_system.cpp"
#include <inttypes.h>

#include "api.h"

static double get_head() {

}

class straight_odom_2_wheel : public sensor_sys { // Make an arc_odom
public:
        straight_odom_2_wheel(int8_t lateral_wheel, int8_t parallel_wheel) : lat_wheel(lateral_wheel), para_wheel(parallel_wheel) {};

        void calc_position() override {
                double a = para_wheel.get_angle() - last_a; // in centidegrees
                double b = lat_wheel.get_angle() - last_b;

                double a_rots = a / (360 * 1000.0);
                double b_rots = b / (360 * 1000.0);

                double a_travel = a_rots * tracking_wheel_circ;
                double b_travel = b_rots * tracking_wheel_circ;

                double theta = get_head(); // global heading
                double cx = a_travel * cos(theta) - b_travel * sin(theta);
                double cy = a_travel * sin(theta) - b_travel * cos(theta);

                x += cx;
                y += cy;

                last_a = a;
                last_b = b;
        }

private:
        pros::Rotation lat_wheel;
        pros::Rotation para_wheel;

        double last_a;
        double last_b;
        double tracking_wheel_circ;
};
