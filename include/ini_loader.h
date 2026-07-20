// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_ini_loader_h
#define tp_ini_loader_h

typedef enum {
        IVT_NUM,
        IVT_STR
} ini_val_type;

typedef struct {
        char* key;
        ini_val_type type;
        union {
                double num;
                char* str;
        } as;
} ini_entry;

typedef struct {
        ini_entry* entries;
} robot_config;

extern robot_config ROBOT_CONFIG;

void get_config(void);

char* get_config_str(char* key);
double get_config_num(char* key);

#endif
