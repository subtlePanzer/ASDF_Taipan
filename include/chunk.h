// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_chunk_h
#define tp_chunk_h

#include "common.h"
#include "value.h"

typedef enum { // Vector OPs?
        OP_CONSTANT, // OP_CONST?
        OP_CONSTANT_LONG,
        OP_NIL,
        OP_TRUE,
        OP_FALSE,
        OP_POP,
        OP_POPN,
        OP_DUP,
        OP_GET_LOCAL,
        OP_GET_LOCAL_LONG,
        OP_GET_GLOBAL,
        OP_GET_GLOBAL_LONG,
        OP_DEFINE_GLOBAL,
        OP_DEFINE_GLOBAL_LONG,
        OP_SET_LOCAL,
        OP_SET_LOCAL_LONG,
        OP_SET_GLOBAL,
        OP_SET_GLOBAL_LONG,
        OP_GET_UPVALUE,
        OP_SET_UPVALUE,
        OP_EQ,
        OP_NEQ,
        OP_GT,
        OP_GT_EQ,
        OP_LT,
        OP_LT_EQ,
        OP_ADD,
        OP_SUBTRACT,
        OP_MULTIPLY,
        OP_DIVIDE,
        OP_NOT,
        OP_NEGATE,
        OP_PRINT_CLI,
        OP_PRINT_SC,
        OP_JU, // jump unconditional
        OP_JIFPT, // Jump if false, pop if true
        OP_JIFPU, // Jump if false, pop unconditional
        OP_JITPF, // Jump if true, pop if false
        OP_JITPU, // Jump if true, pop unconditional
        OP_LOOP,
        OP_CALL,
        OP_CALL_LONG,
        OP_CLOSURE,
        OP_CLOSE_UPVALUE,
        OP_RETURN,
} opcode;

typedef struct {
        int count;
        int capacity;

        uint8_t* code;
        ValueArray constants;

        int* lines;
        int line_count;
        int line_capacity;
} Chunk;

void init_chunk(Chunk* chunk);
void free_chunk(Chunk* chunk);
void write_chunk(Chunk* chunk, uint8_t byte, int line);
void write_constant(Chunk* chunk, Value value, int line);

int get_line(Chunk* chunk, int index);
void append_line(Chunk* chunk, int line);

int add_constant(Chunk* chunk, Value constant);

#endif // tp_chunk_h
