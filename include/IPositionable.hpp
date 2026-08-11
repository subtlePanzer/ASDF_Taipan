#include "auton.h"

#ifndef asdf_ipositionable_h
#define asdf_ipositionable_h

class IPositionable {
public:
        virtual vec2 get_position() = 0;
};

#endif
