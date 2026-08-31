// -----------------------------------------------------------------------------
// ASDF 2026-27 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef asdf_easings_h
#define asdf_easings_h

#include <math.h>

class easing {
        const virtual double get(double t) = 0;
};

class ease_in_cubic : public easing {
        const double get(double t) override {
                return t * t * t;
        }
};

class ease_in_circ : public easing {
        const double get(double t) override {
                return 1 - sqrt(1 - pow(x, 2));
        }
};

class ease_in_quint : public easing {
        const double get(double t) override {
                return t * t * t * t * t;
        }
};

#endif
