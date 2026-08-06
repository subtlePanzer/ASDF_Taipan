#include "sensor_system.cpp"
#include <vector>

class localiser { // eg moving average, etc
public:
        virtual ~localiser() = default;

        virtual void update_pos_estimates() = 0;
        virtual vec2 get_position() const {
                return vec2(x.load(), y.load());
        }

private:
        std::vector<sensor_sys*> data_sources;

protected:
        std::atomic<double> x;
        std::atomic<double> y;
};
