// -----------------------------------------------------------------------------
// ASDF 2026-2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef asdf_units_h
#define asdf_units_h

#include <ratio>

template <typename type, typename internal_type = double, typename conversion_factor = std::ratio<1>>
class value {
public:
        constexpr value(void) {
                internal = 0;
                this_ratio = conversion_factor::num / conversion_factor::den;
        }

        constexpr value(internal_type v) : internal(v) {
                this_ratio = conversion_factor::num / conversion_factor::den;
        }

        template <typename other_ratio>
        value(const length<internal_type, other_ratio>& l) {
                double other_ratio = (double)other_ratio::num / (double)other_ratio::den;

                internal = l.internal() * other_ratio / this_ratio;
        };

        constexpr operator internal_type() const {
                return (internal_type)internal;
        }

        constexpr internal_type internal() const {
                return internal;
        }

        constexpr value operator+(const length_value& l) const {
                return value(internal + l.internal);
        }

        constexpr value operator-(const value& l) const {
                return value(internal - l.internal);
        }

private:
        internal_type internal;
        double this_ratio;
};

struct _length{};
struct _time{};
struct _angle{};

template<typename ratio = std::ratio<1>>
using length = value<_length, double, ratio>;

template<typename ratio = std::ratio<1>>
using time = value<_time, double, ratio>;

template<typename ratio = std::ratio<1>>
using angle = value<_angle, double, ratio>;

using mm = length<std::ratio<1>>; // using millis as base unit
using meters = length<std::ratio<1000, 1>>;
using in = length<std::ratio<10, 254>>;
using ft = length<std::ratio<10, 254 * 12>>;

using sec = time<std::ratio<1000, 1>>;
using msec = time<std::ratio<1>>; // msec is base, as above

using rad = angle<std::ratio<1>>; // #radian_primacy
using deg = angle<std::ratio<57296, 1000>>;
using cdeg = angle<std::ratio<57296, 10000>>;
using rot = angle<std::ratio<628318, 100000>>;

// based on https://en.cppreference.com/cpp/language/user_literal
mm operator"" _mm(long double val) {
        return mm((double)val);
}

meters operator"" _m(long double val) {
        return meters((double)val);
}

in operator"" _in(long double val) {
        return in((double)val);
}

ft operator"" _ft(long double val) {
        return ft((double)val);
}

sec operator"" _sec(long double val) {
        return sec((double)val);
}

msec operator"" _msec(long double val) {
        return msec((double)val);
}

rad operator"" _rads(long double val) {
        return rad((double)val);
}

deg operator"" _deg(long double val) {
        return deg((double)val);
}

cdeg operator"" _cdeg(long double val) {
        return cdeg((double)val);
}

rot operator"" _rots(long double val) {
        return rot((double)val);
}
#endif
