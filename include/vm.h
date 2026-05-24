// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_vm_h
#define tp_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
        ObjClosure* closure;
        uint8_t* ip;
        Value* slots;
} CallFrame;

typedef struct {
        CallFrame frames[FRAMES_MAX];
        int frame_count;

        Value stack[STACK_MAX];
        Value* stack_top;
        Table globals;
        Table strings;
        ObjUpvalue* open_upvalues;

        size_t bytes_allocated;
        size_t next_gc;
        Obj* objects;

        int grey_count;
        int grey_capacity;
        Obj** grey_stack;
} VM;

typedef enum {
        INTERPRET_OK,
        INTERPRET_COMPILE_ERROR,
        INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void init_VM();
void free_VM();
InterpretResult interpret(ObjFunction* function);
void push(Value value);
Value pop();

#endif // tp_vm_h
