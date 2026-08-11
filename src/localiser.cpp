#include "IPositionable.hpp"
#include "sensor_system.hpp"
#include <vector>

class localiser : public IPositionable { // eg moving average, etc
public:
        virtual ~localiser() = default;

        virtual void update_pos_estimates() = 0;

        vec2 get_position() override {
                return vec2(x.load(), y.load());
        }

private:
        std::vector<sensor_sys*> data_sources;

protected:
        std::atomic<double> x;
        std::atomic<double> y;
};
