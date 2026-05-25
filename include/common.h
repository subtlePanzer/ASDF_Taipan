// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_common_h
#define tp_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// EXEC
// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION
// #define DEBUG_MEM_DUMP

// MEM
// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC
// #define USE_CUSTOM_MEMORY

#define UINT24_MAX 0xffffff

#define UINT8_COUNT (UINT8_MAX + 1)
#define UINT24_COUNT (UINT24_MAX + 1)

#ifdef __cplusplus
#include <atomic>
typedef std::atomic_bool lang_agn_atomic_bool;
typedef std::atomic_int lang_agn_atomic_int;
#else
#include <stdatomic.h>
typedef atomic_bool lang_agn_atomic_bool;
typedef atomic_int lang_agn_atomic_int;
#endif

#endif // tp_common_h
