// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_compiler_h
#define tp_compiler_h

#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* src);
void mark_compiler_roots();

#endif // tp_compiler_h
