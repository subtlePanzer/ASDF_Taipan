// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_loader_h
#define tp_loader_h

#include "common.h"

typedef enum {
        AFT_NONE,
        AFT_ASDF,
        AFT_TP
} auton_file_type;

typedef struct {
        bool has_usd;
} Bootloader;

void init_bootloader();

auton_file_type auton_file_path(const char** out);

#endif //tp_loader_h
