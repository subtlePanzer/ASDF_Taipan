// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include "api.h"
#include "object.h"
#include "memory.h"
#include "value.h"

void init_value_array(ValueArray* array) {
        array->values = NULL;
        array->count = 0;
        array->capacity = 0;
}

void write_value_array(ValueArray* array, Value value) {
        printf("");
        if (array->capacity < array->count + 1) {
                int old_cap = array->capacity;
                array->capacity = GROW_CAPACITY(old_cap);
                array->values = GROW_ARRAY(Value, array->values, old_cap,
                        array->capacity);
        }

        array->values[array->count] = value;
        array->count++;
}

void free_value_array(ValueArray* array) {
        FREE_ARRAY(Value, array->values, array->capacity);
        init_value_array(array);
}

void print_value(Value value) {
        switch (value.type) {
                case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
                case VAL_NIL: printf("nil"); break;
                case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
                case VAL_OBJ: print_object(value); break;
                case VAL_PORT: printf("<port %d>", AS_PORT(value));
                case VAL_MG: printf("<motorgroup %d %d %d>",
                        AS_MG(value)[0],
                        AS_MG(value)[1],
                        AS_MG(value)[2]);
        }
}

void screen_print_value(Value value) {
        switch (value.type) {
                case VAL_BOOL:
                        screen_print(E_TEXT_MEDIUM, 0, AS_BOOL(value) ? "true" : "false");
                        break;
                case VAL_NIL: screen_print(E_TEXT_MEDIUM, 0, "nil");
                        break;
                case VAL_NUMBER: screen_print(E_TEXT_MEDIUM, 0, "%g", AS_NUMBER(value));
                        break;
                case VAL_OBJ:
                        screen_print(E_TEXT_MEDIUM, 0, stringify_object(value));
                        break;
                case VAL_PORT:
                        screen_print(E_TEXT_MEDIUM, 0, "<port %d>", AS_PORT(value));
                case VAL_MG:
                        screen_print(E_TEXT_MEDIUM, 0, 
                                "<motorgroup %d %d %d>",
                                AS_MG(value)[0],
                                AS_MG(value)[1],
                                AS_MG(value)[2]);
        }
}

bool values_eq(Value a, Value b) {
        if (a.type != b.type) return false;
        switch (a.type) {
                case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
                case VAL_NIL:    return true;
                case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
                case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);
                case VAL_PORT:   return AS_PORT(a) == AS_PORT(b);
                case VAL_MG: {
                        int8_t ports_a[3] = AS_MG(a);
                        int8_t ports_b[3] = AS_MG(b);

                        return ports_a[0] == ports_b[0]
                                && ports_a[1] == ports_b[1]
                                && ports_a[2] == ports_b[2];
                }
                default:         return false; // unreachable
        }
}
