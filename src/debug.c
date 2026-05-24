#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "object.h"
#include "memory.h"
#include "value.h"

extern MemManager mem_manager;

static void dump_block(void* addr, bool is_sentinel, bool is_free, size_t capacity) {
        printf("  0x%-16p", addr);
        printf("       "); // 7
        printf("[%s]", is_sentinel ? "S" : "B");
        printf("      "); // 6
        printf("%s", is_free ? is_sentinel ? "ERROR" : "    FREE" : is_sentinel ? "SENTINEL" : "OCCUPIED");
        printf("    "); // 4
        printf("%10zu bytes", capacity);
        printf("      "); // 6
        printf("%10zu bytes\n", capacity + sizeof(MemHeader));
}

void memory_dump() { // 18 char address (0x16), 6 char space, type = ' [T]'
        printf("\n==== MEMORY DUMP ====\n");
        printf("  ADDRESS                 TYPE       STATUS      CAPACITY              TOTAL SIZE\n");
        printf("  -------------------------------------------------------------------------------------\n");

        MemHeader* header_addr = mem_manager.first_head;

        int num_blocks = 0;
        int num_free = 0;
        size_t payload = 0;
        size_t header_volume = 0;

        while (true) {
                dump_block((void*)header_addr, HDR_IS_BOUNDARY(header_addr), HDR_IS_FREE(header_addr), HDR_SIZE(header_addr));
                num_blocks++;

                if (HDR_IS_FREE(header_addr)) num_free++;
                else payload += HDR_SIZE(header_addr);

                header_volume += (HDR_IS_BOUNDARY(header_addr) ? sizeof(MemHeader) : 2 * sizeof(MemHeader));

                if (HDR_IS_BOUNDARY(header_addr) && (void*)header_addr != mem_manager.mem_start) break;
                else header_addr = HDR_NEXT(header_addr);
        }

        double density = ((double)payload / (payload + header_volume)) * 100.0;

        printf("  ------------------------------------------------------------------------\n");
        printf("SUMMARY: %3i Blocks (%3i Free), %8zu bytes payload, %5.2f%% density\n", num_blocks, num_free, payload, density);
}

void stack_trace(VM* vm) { // can  also reverse
        for (int i = vm->frame_count - 1; i >= 0; i--) {
                CallFrame* frame = &vm->frames[i];
                ObjFunction* function = frame->closure->function;
                size_t instruction = frame->ip - function->chunk.code - 1;
                fprintf(stderr, "[line %d] in ",
                        get_line(&function->chunk, instruction));

                if (function->name == NULL)
                        fprintf(stderr, "script\n");
                else
                        fprintf(stderr, "%s()\n", function->name->chars);

        }
}
void disassemble_chunk(Chunk* chunk, const char* name) {
        printf("== %s ==\n", name);

        for (int offset = 0; offset < chunk->count;) {
                offset = disassemble_instruction(chunk, offset);
        }
}

static int simple_instruction(const char* name, int offset) {
        printf("%s\n", name);
        return offset + 1;
}

static int byte_instruction(const char* name, Chunk* chunk, int offset) {
        uint8_t slot = chunk->code[offset + 1];
        printf("%-16s %8d\n", name, slot);
        return offset + 2;
}

static int long_byte_instruction(const char* name, Chunk* chunk, int offset) {
        uint32_t slot1 = (uint32_t)chunk->code[offset + 1];
        uint32_t slot2 = (uint32_t)chunk->code[offset + 2];
        uint32_t slot3 = (uint32_t)chunk->code[offset + 3];

        uint32_t slot = slot1 | (slot2 << 8) | (slot3 << 16);

        printf("%-16s %8u\n", name, slot);

        return offset + 4;
}

static int jump_instruction(const char* name, int sign, Chunk* chunk, int offset) {
        uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
        jump |= chunk->code[offset + 2];

        printf("%-16s %8u -> %d\n", name, offset, offset + 3 + sign * jump);

        return offset + 3;
}

static int constant_instruction(const char* name, Chunk* chunk, int offset) {
        uint8_t constant = chunk->code[offset + 1];
        printf("%-16s %8d '", name, constant);
        print_value(chunk->constants.values[constant]);
        printf("'\n");
        return offset + 2;
}

static int long_constant_instruction(const char* name, Chunk* chunk,
        int offset) {
        uint32_t constant1 = (uint32_t)chunk->code[offset + 1];
        uint32_t constant2 = (uint32_t)chunk->code[offset + 2];
        uint32_t constant3 = (uint32_t)chunk->code[offset + 3];

        uint32_t index = constant1
                | (constant2 << 8)
                | (constant3 << 16);

        printf("%-16s %8u '", name, index);
        print_value(chunk->constants.values[index]);
        printf("'\n");
        return offset + 4;
}

int disassemble_instruction(Chunk* chunk, int offset) {
        printf("%04d ", offset);
        if (offset > 0 && get_line(chunk, offset) == get_line(chunk, offset - 1))
                printf("   | ");
        else
                printf("%4d ", get_line(chunk, offset));


        uint8_t instruction = chunk->code[offset];
        switch (instruction) {
                case OP_CONSTANT:
                        return constant_instruction("OP_CONSTANT", chunk,
                                offset);
                case OP_CONSTANT_LONG:
                        return long_constant_instruction("OP_CONSTANT_LONG",
                                chunk, offset);
                case OP_NIL:
                        return simple_instruction("OP_NIL", offset);
                case OP_TRUE:
                        return simple_instruction("OP_TRUE", offset);
                case OP_FALSE:
                        return simple_instruction("OP_FALSE", offset);
                case OP_POP:
                        return simple_instruction("OP_POP", offset);
                case OP_POPN:
                        return byte_instruction("OP_POPN", chunk, offset);
                case OP_DUP:
                        return simple_instruction("OP_DUP", offset);
                case OP_GET_LOCAL:
                        return byte_instruction("OP_GET_LOCAL", chunk, offset);
                case OP_GET_LOCAL_LONG:
                        return long_byte_instruction("OP_GET_LOCAL_LONG", chunk,
                                offset);
                case OP_GET_GLOBAL:
                        return constant_instruction("OP_GET_GLOBAL", chunk,
                                offset);
                case OP_GET_GLOBAL_LONG:
                        return long_constant_instruction("OP_GET_GLOBAL_LONG",
                                chunk, offset);
                case OP_DEFINE_GLOBAL:
                        return constant_instruction("OP_DEFINE_GLOBAL", chunk,
                                offset);
                case OP_DEFINE_GLOBAL_LONG:
                        return long_constant_instruction("OP_DEFINE_GLOBAL_LONG",
                                chunk, offset);
                case OP_SET_LOCAL:
                        return byte_instruction("OP_SET_LOCAL", chunk, offset);
                case OP_SET_LOCAL_LONG:
                        return long_byte_instruction("OP_SET_LOCAL_LONG", chunk,
                                offset);
                case OP_SET_GLOBAL:
                        return constant_instruction("OP_SET_GLOBAL", chunk,
                                offset);
                case OP_SET_GLOBAL_LONG:
                        return long_constant_instruction("OP_SET_GLOBAL_LONG",
                                chunk, offset);
                case OP_GET_UPVALUE:
                        return byte_instruction("OP_GET_UPVALUE", chunk, offset);
                case OP_SET_UPVALUE:
                        return byte_instruction("OP_SET_UPVALUE", chunk, offset);
                case OP_EQ:
                        return simple_instruction("OP_EQ", offset);
                case OP_NEQ:
                        return simple_instruction("OP_NEQ", offset);
                case OP_GT:
                        return simple_instruction("OP_GT", offset);
                case OP_GT_EQ:
                        return simple_instruction("OP_GT_EQ", offset);
                case OP_LT:
                        return simple_instruction("OP_LT", offset);
                case OP_LT_EQ:
                        return simple_instruction("OP_LT_EQ", offset);
                case OP_ADD:
                        return simple_instruction("OP_ADD", offset);
                case OP_SUBTRACT:
                        return simple_instruction("OP_SUBTRACT", offset);
                case OP_MULTIPLY:
                        return simple_instruction("OP_MULTIPLY", offset);
                case OP_DIVIDE:
                        return simple_instruction("OP_DIVIDE", offset);
                case OP_NOT:
                        return simple_instruction("OP_NOT", offset);
                case OP_NEGATE:
                        return simple_instruction("OP_NEGATE", offset);
                case OP_PRINT_CLI:
                        return simple_instruction("OP_PRINT_CLI", offset);
                case OP_PRINT_SC:
                        return simple_instruction("OP_PRINT_SC", offset);
                case OP_JU:
                        return jump_instruction("OP_JU", 1, chunk, offset);
                case OP_JIFPT:
                        return jump_instruction("OP_JIFPT", 1, chunk, offset);
                case OP_JIFPU:
                        return jump_instruction("OP_JIFPU", 1, chunk, offset);
                case OP_JITPF:
                        return jump_instruction("OP_JITPF", 1, chunk, offset);
                case OP_JITPU:
                        return jump_instruction("OP_JITPU", 1, chunk, offset);
                case OP_LOOP:
                        return jump_instruction("OP_LOOP", -1, chunk, offset);
                case OP_CALL:
                        return byte_instruction("OP_CALL", chunk, offset);
                case OP_CALL_LONG:
                        return long_byte_instruction("OP_CALL_LONG", chunk,
                                offset);
                case OP_CLOSURE: {
                        offset++;
                        uint8_t constant = chunk->code[offset++];
                        printf("%-16s %4d ", "OP_CLOSURE", constant);
                        print_value(chunk->constants.values[constant]);
                        printf("\n");

                        ObjFunction* function = AS_FUNCTION(
                                chunk->constants.values[constant]);
                        for (int j = 0; j < function->upvalue_count; j++) {
                                int isLocal = chunk->code[offset++];
                                int index = chunk->code[offset++];
                                printf("%04d      |                     %s %d\n",
                                        offset - 2, isLocal ? "local" : "upvalue", index);
                        }

                        return offset;
                }
                case OP_CLOSE_UPVALUE:
                        return simple_instruction("OP_CLOSE_UPVALUE", offset);
                case OP_RETURN:
                        return simple_instruction("OP_RETURN", offset);


                default:
                        printf("Unknown opcode %d\n", instruction);
                        return offset + 1;
        }
}
