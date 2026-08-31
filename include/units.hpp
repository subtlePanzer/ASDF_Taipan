// -----------------------------------------------------------------------------
// ASDF 2026-2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef asdf_units_h
#define asdf_units_h

#include <ratio>

template <typename internal_type, typename conversion_factor = std::ratio<1>>
class length_value {
public:
        constexpr length_value(void) {
                internal = 0;
                this_ratio = conversion_factor::num / conversion_factor::den;
        }

        constexpr length_value(internal_type v) : internal(v) {
                this_ratio = conversion_factor::num / conversion_factor::den;
        }

        template <typename other_ratio>
        length_value(const length<internal_type, ratio>& l) {
                double other_ratio = (double)other_ratio::num / (double)other_ratio::den;

                internal = l.internal() * other_ratio / this_ratio;
        };

        constexpr operator internal_type() const {
                return (internal_type)internal;
        }

        constexpr internal_type internal() const {
                return internal;
        }

        constexpr length_value operator+(const length_value& l) const {
                return length_value(internal + l.internal);
        }

        constexpr length_value operator-(const length_value& l) const {
                return length_value(internal - l.internal);
        }

private:
        internal_type internal;
        double this_ratio;
};

using meters = length_value<double, std::ratio<1000, 1>>;
using mm = length_value<double, std::ratio<1>>; // using millis as base unit
using in = length_value<double, std::ratio<10, 254>>;
using ft = length_value<double, std::ratio<10, 254 * 12>>;

// add custom literals

#endif
