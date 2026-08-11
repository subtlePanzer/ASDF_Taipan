#pragma once


#include "auton.h"
#include <math.h>

#define RAD2DEG (M_PI / 180)

static float get_mag(vec2 v) {
        return sqrt(v.x * v.x + v.y * v.y);
}

static float shd(float h1, float h2) { // signed heading difference
        float value = fmod(h2 - h1 + 180, 360);
        if (value < 0)
                value += 360;
        return value - 180;
}

static float argd(vec2 v) {
        return atan2(v.y, v.x) * RAD2DEG;
}

static float clamp(float v, float minv, float maxv) {
        return fmax(v, fmin(v, maxv));
}
