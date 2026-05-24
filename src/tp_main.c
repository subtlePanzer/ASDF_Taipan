#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "chunk.h"
#include "debug.h"
#include "tp_loader.h"
#include "tp_main.h"
#include "memory.h"
#include "vm.h"

static char* read_file(const char* path) {
        FILE* file = fopen(path, "rb");

        if (file == NULL) {
                fprintf(stderr, "Could not open file \"%s\".\n", path);
                exit(74);
        }

        fseek(file, 0L, SEEK_END);
        size_t file_size = ftell(file);
        rewind(file);

        char* buffer = (char*)malloc(file_size + 1);
        if (buffer == NULL) {
                fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
                exit(74);
        }
        size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
        if (bytes_read < file_size) {
                fprintf(stderr, "Could not read file \"%s\".\n", path);
                exit(74);
        }
        buffer[bytes_read] = '\0';

        fclose(file);
        return buffer;
}

ObjFunction* compile_file(const char* path) {
        char* source = read_file(path);
        ObjFunction* func = compile(source);
        free(source);
        return func;
}

ObjFunction* bytecode_file_to_raw_bc(const char* bytecode) {
        ObjFunction* func;
        fprintf(stderr, "Reading raw bytecode is not implemented yet.\n");
        return func;
}

ObjFunction* read_tp_file(const char* path) {
        char* bytecode = read_file(path);
        ObjFunction* func = bytecode_file_to_raw_bc(bytecode);
        free(bytecode);
        return func;
}

void run_script(ObjFunction* function) {
        InterpretResult result = interpret(function);

        if (result == INTERPRET_COMPILE_ERROR) exit(65);
        if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int init_tp() {
        printf("🐍 Initing Taipan...\n");

#ifdef USE_CUSTOM_MEMORY
        init_mem_manager();
        printf("🐍 Inited memory manager...\n");
#endif

        init_VM();
        printf("🐍 Inited VM...\n");

        init_bootloader();
        printf("🐍 Inited Bootloader...\n");

        const char* path;
        auton_file_type result = auton_file_path(&path);

        switch (result) {
                case AFT_NONE:
                        printf("🐍 No auton script found.\n");
                        printf("🐍 Aborting...\n");
                        return -1;

                case AFT_ASDF:
                        printf("🐍 Compiling 'auton.asdf'...");
                        ObjFunction* compiled = compile_file(path);
                        printf("compiled.\n");
                        printf("🐍 Running 'auton.asdf'...\n");
                        run_script(compiled);
                        printf("🐍 Finished.\n");
                        break;

                case AFT_TP:
                        printf("🐍 Running 'out.tp'...\n");
                        run_script(read_tp_file(path));
                        printf("🐍 Finished.\n");
                        break;

                default:
                        return -1; // Unreachable
        }

        free_VM();
        return 0;
}
