// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

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

void run_script(void* params) {
        run_script_params* args = (run_script_params*)params;
        ObjFunction* function = args->func;
        void* abort_flag = args->interrupt;
        void* main_handle = args->main_handle;
        void* vm_cleanup = args->vm_cleanup_atomic;

        InterpretResult result = interpret(function, abort_flag, main_handle, vm_cleanup);

        if (result == INTERPRET_COMPILE_ERROR) exit(65);
        if (result == INTERPRET_RUNTIME_ERROR) exit(70);

        return;
}

ObjFunction* init_tp() {
        printf("🐍 Initing Taipan...\n");

        printf("🐍 Initing VM... ");
        init_VM();
        printf("inited.\n");


#ifdef USE_CUSTOM_MEMORY
        printf("🐍 Inited memory manager... ");
        init_mem_manager();
        printf("inited.\n");
#endif

        printf("🐍 Inited Bootloader... ");
        init_bootloader();
        printf("inited.\n");

        const char* path;
        auton_file_type result = auton_file_path(&path);

        switch (result) {
                case AFT_NONE:
                        printf("🐍 No auton script found.\n");
                        printf("🐍 Skipping auton...\n"); // could fallback to builtin default
                        return NULL;

                case AFT_ASDF:
                        printf("🐍 Compiling 'auton.asdf'... ");
                        ObjFunction* compiled = compile_file(path);
                        printf("compiled.\n");
                        return compiled;

                case AFT_TP:
                        return read_tp_file(path);

                default:
                        return NULL;
        }
        return NULL; // Unreachable
}
