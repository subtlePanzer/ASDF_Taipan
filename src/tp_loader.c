// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include "api.h"
#include "tp_loader.h"
#include "memory.h"

#include <stdio.h>

// read file manifest
// Pull most recent .asdf or .tp file
// if .tp:
//    execute bytecode
// if .asdf:
//    parse
//    compile
//    execute

Bootloader bootloader;

void init_bootloader() {
        bootloader.has_usd = !!usd_is_installed();
}

static size_t get_fsize(FILE* file) {
        fseek(file, 0L, SEEK_END);
        size_t fsize = ftell(file);
        rewind(file);

        return fsize;
}

auton_file_type auton_file_path(const char** out) {
        *out = "";

        if (!bootloader.has_usd) {
                printf("🐍 SD Card not found.\n");
                return AFT_NONE;
        }

        printf("🐍 SD Card found...\n");

        // check for bytecode
        FILE* dot_tp_file = fopen("/usd/out.tp", "rb");
        if (dot_tp_file != NULL)
        {
                printf("🐍 Found 'out.tp' (bytecode).\n");
                fclose(dot_tp_file);
                *out = "/usd/out.tp";
                return AFT_TP;
        }

        // check for src
        FILE* dot_asdf_file = fopen("/usd/auton.asdf", "r");
        if (dot_asdf_file != NULL)
        {
                printf("🐍 Found 'auton.asdf' (src).\n");
                fclose(dot_asdf_file);
                *out = "/usd/auton.asdf";
                return AFT_ASDF;
        }

        return AFT_NONE;
}
