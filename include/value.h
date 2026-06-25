// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_value_h
#define tp_value_h

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum {
        VAL_BOOL,
        VAL_NIL,
        VAL_NUMBER,
        VAL_OBJ,
        VAL_PORT,
        VAL_MG,
} ValueType;

typedef struct {
        ValueType type;
        union {
                bool boolean;
                double number;
                Obj* obj;
                int8_t port;
                int8_t mg[3]; // Hard cap of 3 motors per group
        } as;
} Value;

#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value)    ((value).type == VAL_OBJ)
#define IS_PORT(value)   ((value).type == VAL_PORT)
#define IS_MG(value)     ((value).type == VAL_MG)

#define AS_OBJ(value)    ((value).as.obj)
#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_PORT(value)   ((value).as.port)
#define AS_MG(value)     ((value).as.mg)

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})
#define PORT_VAL(value)   ((Value){VAL_PORT, {.port = value}})
#define MG_VAL(value)     ((Value){VAL_MG, {.val = value}})

typedef struct {
        int capacity;
        int count;
        Value* values;
} ValueArray;

bool values_eq(Value a, Value b);
void init_value_array(ValueArray* array);
void write_value_array(ValueArray* array, Value value);
void free_value_array(ValueArray* array);
void print_value(Value value);
void screen_print_value(Value value);

#endif // clox_value_h
