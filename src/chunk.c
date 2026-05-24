// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h"

void init_chunk(Chunk* chunk) {
        chunk->count = 0;
        chunk->capacity = 0;
        chunk->code = NULL;
        chunk->line_count = 0;
        chunk->line_capacity = 0;
        chunk->lines = NULL;

        init_value_array(&chunk->constants);
}


void write_chunk(Chunk* chunk, uint8_t byte, int line) {
        if (chunk->capacity < chunk->count + 1) {
                int old_cap = chunk->capacity;
                chunk->capacity = GROW_CAPACITY(old_cap);
                chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_cap,
                        chunk->capacity);
        }

        chunk->code[chunk->count] = byte;

        append_line(chunk, line); // IS AN APPEND, NOT A WRITE
        // chunk->lines[chunk->count] = line; // won't work with RLE

        chunk->count++;
}

void write_constant(Chunk* chunk, Value value, int line) {
        int index = add_constant(chunk, value);

        if (index < 256) { // make 0 to force OP_CONSTANT_LONG
                write_chunk(chunk, OP_CONSTANT, line);
                write_chunk(chunk, (uint8_t)index, line); // force index into a byte
        } else { // little endian
                write_chunk(chunk, OP_CONSTANT_LONG, line);
                write_chunk(chunk, (uint8_t)(index & 0xff), line);
                write_chunk(chunk, (uint8_t)((index >> 8) & 0xff), line);
                write_chunk(chunk, (uint8_t)((index >> 16) & 0xff), line);
        } // error handling
}

void free_chunk(Chunk* chunk) {
        FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
        FREE_ARRAY(int, chunk->lines, chunk->line_capacity);
        free_value_array(&chunk->constants);
        init_chunk(chunk);
}

int add_constant(Chunk* chunk, Value value) {
        push(value);
        write_value_array(&chunk->constants, value);
        pop();
        return chunk->constants.count - 1;
}

void append_line(Chunk* chunk, int line) { // O(1) in time and memory!
        if (chunk->line_capacity < chunk->line_count + 1) {
                int old_cap = chunk->line_capacity;
                chunk->line_capacity = GROW_CAPACITY(old_cap);
                chunk->lines = GROW_ARRAY(int, chunk->lines, old_cap,
                        chunk->line_capacity);
        }

        if (chunk->line_count == 0)
        {
                chunk->lines[chunk->line_count++] = line;
                return;
        }

        int last_index = chunk->line_count - 1;


        if (chunk->lines[last_index] < 0
                && chunk->lines[last_index - 1] == line)
                chunk->lines[last_index]--;
        else if (chunk->lines[last_index] == line)
                chunk->lines[chunk->line_count++] = -1;
        else
                chunk->lines[chunk->line_count++] = line;
}

int get_line(Chunk* chunk, int index) { // O(n), only called on err
        int curr = 0;
        int last = -1;

        for (int i = 0; i < chunk->line_count; i++) {
                int entry = chunk->lines[i];
                if (entry >= 0) {
                        last = entry;
                        if (curr == index)
                                return last;

                        curr++;
                } else {
                        int repeat = -entry;
                        if (index >= curr && index < curr + repeat)
                                return last;

                        curr += repeat;
                }
        }

        return -1;
}
