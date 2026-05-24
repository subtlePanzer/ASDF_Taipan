#ifndef tp_debug_h
#define tp_debug_h

#include "chunk.h"
#include "vm.h"

void memory_dump();
void stack_trace(VM* vm);

void disassemble_chunk(Chunk* chunk, const char* name);
int disassemble_instruction(Chunk* chunk, int offset);

#endif // tp_debug_h
