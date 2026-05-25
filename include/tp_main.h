// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_main_h
#define tp_main_h

#include "api.h"
#include "object.h"

typedef struct {
        ObjFunction* func;
        void* interrupt;
        void* vm_cleanup_atomic;
        void* main_handle;
} run_script_params;

ObjFunction* init_tp();
void run_script(void* params);

#endif // tp_main_h
