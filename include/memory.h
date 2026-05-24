#ifndef tp_memory_h
#define tp_memory_h

#include "common.h"
#include "object.h"

#define GROW_CAP_BASE_SCALE 8
#define GROW_CAP_SCALE_FACTOR 2
#define MIN_ALLOC_SIZE ((2 * sizeof(void*) + sizeof(MemHeader) + ALIGNMENT_MASK) & ~ALIGNMENT_MASK) // 2 ptrs for free block
#define ALIGNMENT 8
#define ALIGNMENT_MASK (ALIGNMENT - 1)

#define FREE_BIT 0x1
#define PREV_FREE_BIT 0x2
#define SIZE_MASK (~(size_t)3)

#define HDR_IS_FREE(header)      (!!((header)->size_flags & FREE_BIT))
#define HDR_PREV_IS_FREE(header) (!!((header)->size_flags & PREV_FREE_BIT))
#define HDR_SIZE(header)         ((header)->size_flags & SIZE_MASK)
#define HDR_NEXT(header)         ((MemHeader*)((char*)(header) + sizeof(MemHeader) + HDR_SIZE(header)))

#define ALLOCATE(type, count) \
        (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0);

#define GROW_CAPACITY(capacity) \
        ((capacity) < GROW_CAP_BASE_SCALE \
        ? GROW_CAP_BASE_SCALE \
        : (capacity)*GROW_CAP_SCALE_FACTOR) // can adjust scale factor

#define GROW_ARRAY(type, pointer, old_count, new_count) \
        (type*)reallocate(pointer, sizeof(type) * (old_count), \
        sizeof(type) * (new_count))

#define FREE_ARRAY(type, pointer, old_count) \
        reallocate(pointer, sizeof(type) * (old_count), 0)

typedef struct {
        size_t size_flags; // payload size
} MemHeader; // needs compression, tonnes of cool tricks

typedef struct FreeBlock {
        MemHeader header;
        struct FreeBlock* next;
        struct FreeBlock* prev;
} FreeBlock;

typedef struct {
        void* mem_start;
        MemHeader* first_head;
        FreeBlock* first_free;
} MemManager;

extern void* mem_end;
extern MemManager mem_manager;

#define HDR_IS_BOUNDARY(header)  (header == mem_manager.mem_start || header == mem_end)

void init_mem_manager();
void make_header(MemHeader* header, size_t size, bool is_free, bool is_prev_free);

void* reallocate(void* pointer, size_t old_size, size_t new_size);
void mark_obj(Obj* obj);
void mark_value(Value value);
void collect_garbage();
void free_objects();

#endif // tp_memory_h
