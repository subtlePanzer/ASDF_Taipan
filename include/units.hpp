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
                _internal = 0;
                this_ratio = conversion_factor::num / conversion_factor::den;
        }

        constexpr value(internal_type v) : _internal(v) {
                this_ratio = conversion_factor::num / conversion_factor::den;
        }

        template <typename other_ratio>
        value(const value<type, internal_type, other_ratio>& l) {
                double ratio_double = (double)other_ratio::num / (double)other_ratio::den;

                _internal = l.internal() * ratio_double / this_ratio;
        };

        constexpr operator internal_type() const {
                return (internal_type)_internal;
        }

        constexpr internal_type internal() const {
                return _internal;
        }

        constexpr value operator+(const value& l) const {
                return value(_internal + l._internal);
        }

        constexpr value operator-(const value& l) const {
                return value(_internal - l._internal);
        }

private:
        internal_type _internal;
        double this_ratio;
};

struct _length{};
struct _time{};
struct _angle{};

template<typename ratio = std::ratio<1>>
using length = value<_length, double, ratio>;

template<typename ratio = std::ratio<1>>
using asdf_time = value<_time, double, ratio>;

template<typename ratio = std::ratio<1>>
using angle = value<_angle, double, ratio>;

using mm = length<std::ratio<1>>; // using millis as base unit
using meters = length<std::ratio<1000, 1>>;
using in = length<std::ratio<10, 254>>;
using ft = length<std::ratio<10, 254 * 12>>;

using asdf_sec = asdf_time<std::ratio<1000, 1>>;
using asdf_msec = asdf_time<std::ratio<1>>; // msec is base, as above

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

asdf_sec operator"" _sec(long double val) {
        return asdf_sec((double)val);
}

asdf_msec operator"" _msec(long double val) {
        return asdf_msec((double)val);
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
