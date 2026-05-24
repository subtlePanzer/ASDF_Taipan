#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

#define GC_HEAP_GROW_FACTOR 2 // TODO: tune for vex

#define HDR_SET_SIZE(header, size)            (header)->size_flags = ((size) & SIZE_MASK) | ((header)->size_flags & ~SIZE_MASK)
#define HDR_SET_FLAG(header, free, prev_free) (header)->size_flags = ((header)->size_flags & SIZE_MASK) \
                                                        | ((free) ? FREE_BIT : 0) \
                                                        | ((prev_free) ? PREV_FREE_BIT : 0)

MemManager mem_manager;

void* mem_end = NULL;

static void add_free_block(FreeBlock* block) {
        block->next = mem_manager.first_free;
        block->prev = NULL;
        if (mem_manager.first_free != NULL)
                mem_manager.first_free->prev = block;

        mem_manager.first_free = block;
}

static void rm_free_block(FreeBlock* block) {
        if (block == NULL) return;

        if (block->prev != NULL)
                block->prev->next = block->next;
        else
                mem_manager.first_free = block->next;

        if (block->next != NULL)
                block->next->prev = block->prev;
}

void init_mem_manager() {
#define MB (1024 * 1024)
#define BASE_HEAP_SIZE (32 * MB)
        void* memory = malloc(BASE_HEAP_SIZE);
        size_t size = BASE_HEAP_SIZE - (3 * sizeof(MemHeader));

        MemHeader* header = (MemHeader*)(memory);
        MemHeader* footer = (MemHeader*)((char*)memory + size);
        MemHeader* sentinel = (MemHeader*)(footer + 1);

        make_header(header, size, true, false);
        make_header(footer, size, true, false);
        make_header(sentinel, 0, false, true);
        mem_manager.mem_start = memory;
        mem_manager.first_head = header;
        mem_end = (void*)sentinel;

        mem_manager.first_free = (FreeBlock*)header;
        mem_manager.first_free->next = NULL;
        mem_manager.first_free->prev = NULL;

#undef BASE_HEAP_SIZE
#undef MB
}

void make_header(MemHeader* header, size_t size, bool is_free, bool is_prev_free) {
        HDR_SET_SIZE(header, size);
        HDR_SET_FLAG(header, is_free, is_prev_free);
}

static void assert(bool stmt, const char* msg) {
        if (!stmt)
        {
                fprintf(stderr, "Assert failed: \"%s\"\n", msg);
#ifdef USE_CUSTOM_MEMORY
                memory_dump();
#endif // USE_CUSTOM_MEMORY
                exit(70);
        }
}

static void coalesce(MemHeader* header) {
        bool is_prev_free = HDR_PREV_IS_FREE(header);
        size_t size = HDR_SIZE(header);

        if (is_prev_free) {
                // coalesce with prev footer
                MemHeader* prev_footer = header - 1;
                size_t prev_size = HDR_SIZE(prev_footer);
                MemHeader* prev_header = (MemHeader*)((char*)prev_footer - prev_size);

                rm_free_block((FreeBlock*)prev_header);

                size += prev_size + sizeof(MemHeader);
                HDR_SET_SIZE(prev_header, size);
                header = prev_header;
        }

        MemHeader* next = HDR_NEXT(header);
        if (!HDR_IS_BOUNDARY(next) && HDR_IS_FREE(next)) {
                rm_free_block((FreeBlock*)next);
                size += HDR_SIZE(next) + sizeof(MemHeader);
                HDR_SET_SIZE(header, size);
        }

        HDR_SET_FLAG(header, true, HDR_PREV_IS_FREE(header));

        MemHeader* foot = (MemHeader*)((char*)header + size);
        make_header(foot, size, true, HDR_PREV_IS_FREE(header));

        MemHeader* next_next = HDR_NEXT(header);
        if (!HDR_IS_BOUNDARY(next_next))
                HDR_SET_FLAG(next_next, HDR_IS_FREE(next_next), true);
        else
                next_next->size_flags |= PREV_FREE_BIT;

        add_free_block((FreeBlock*)header);
}

// split blocks
static void split_block(FreeBlock* block, size_t size) {
        assert(size + 2 * sizeof(MemHeader) >= MIN_ALLOC_SIZE, "Can't make a block that small.");

        size_t total = HDR_SIZE(&block->header);

        if (total >= MIN_ALLOC_SIZE + sizeof(MemHeader) + size) {
                size_t remaining = total - size - sizeof(MemHeader);

                MemHeader* block1 = &block->header;
                HDR_SET_SIZE(block1, size);
                HDR_SET_FLAG(block1, HDR_IS_FREE(block1), HDR_PREV_IS_FREE(block1));

                MemHeader* block2 = HDR_NEXT(block1);
                HDR_SET_SIZE(block2, remaining);
                HDR_SET_FLAG(block2, true, false);

                MemHeader* block2_foot = (MemHeader*)((char*)block2 + remaining);
                HDR_SET_SIZE(block2_foot, remaining);
                HDR_SET_FLAG(block2_foot, true, false);

                MemHeader* next = HDR_NEXT(block2);
                if (!HDR_IS_BOUNDARY(next))
                        HDR_SET_FLAG(next, HDR_IS_FREE(next), true);
                else
                        next->size_flags |= PREV_FREE_BIT;

                add_free_block((FreeBlock*)block2);

        }
}

void* taipan_realloc(void* ptr, size_t old_size, size_t new_size) {
        // If sizes are the same, return pointer
        // Check for and free if new_size = 0
        //      Coalesce following and prev block
        //      Return null
        // Check for shrink
        //    Shrink in place and return
        // Check if can grow in place
        //    grow in place and return
        // else
        //    look for a new block
        //    if can't find one, fail
        //    if found one:
        //        copy data, free old, and return

        // free
        if (new_size == 0) {
                if (ptr != NULL) {
                        MemHeader* header = ((MemHeader*)ptr - 1);
                        coalesce(header);
                }
                return NULL;
        }

        if (new_size < MIN_ALLOC_SIZE) new_size = MIN_ALLOC_SIZE;
        new_size = (new_size + ALIGNMENT_MASK) & ~ALIGNMENT_MASK; // align to boundaries

        if (new_size == old_size)
                return ptr;

        // check if no memory allocated (prevents nullptr deref)
        if (ptr == NULL || old_size == 0) {
                // allocate new memory
                // look for another block
                FreeBlock* curr_block = mem_manager.first_free;
                while (curr_block != NULL) {
                        if (HDR_SIZE(&curr_block->header) >= new_size) {
                                rm_free_block(curr_block);
                                split_block(curr_block, new_size);

                                MemHeader* new_head = &curr_block->header;
                                HDR_SET_FLAG(new_head, false, HDR_PREV_IS_FREE(new_head));

                                MemHeader* next = HDR_NEXT(new_head);
                                if (!HDR_IS_BOUNDARY(next))
                                        HDR_SET_FLAG(next, HDR_IS_FREE(next), false);
                                else
                                        next->size_flags &= ~PREV_FREE_BIT;
                                return (void*)(new_head + 1);
                        }
                        curr_block = curr_block->next;
                }
                fprintf(stderr, "Heap overflow.\n");
                memory_dump();
                exit(1); // heap overflow
        }

        MemHeader* curr = ((MemHeader*)ptr) - 1;
        if (new_size <= HDR_SIZE(curr))
                return ptr; // fast shrink, unlikely to be used much


        // // shrink in place
        // if (new_size < HDR_SIZE(curr_header)) {
        //         // check how much we're cutting off
        //         if (HDR_SIZE(curr_header) - new_size >= MIN_ALLOC_SIZE) {
        //                 split_block(curr_header, new_size, false, true);

        //                 // coalesce
        //                 MemHeader* remaining_head = HDR_NEXT(curr_header);
        //                 MemHeader* remaining_foot = (MemHeader*)((char*)remaining_head + HDR_SIZE(remaining_head) + sizeof(MemHeader));
        //                 coalesce(remaining_head, remaining_foot);
        //         }
        //         return ptr;
        // }

        // grow in place: following (does not merge with previous and shuffle memory back)
        MemHeader* next = HDR_NEXT(curr);
        if (!HDR_IS_BOUNDARY(next) && HDR_IS_FREE(next)
                && (HDR_SIZE(curr) + HDR_SIZE(next) + sizeof(MemHeader) >= new_size)) {
                rm_free_block((FreeBlock*)next);
                size_t total_free = HDR_SIZE(curr) + HDR_SIZE(next) + sizeof(MemHeader);

                if (total_free - new_size >= MIN_ALLOC_SIZE + sizeof(MemHeader)) {
                        // split
                        size_t leftover = total_free - sizeof(MemHeader) - new_size;

                        HDR_SET_SIZE(curr, new_size);

                        // make new header
                        MemHeader* remainder = HDR_NEXT(curr);
                        HDR_SET_SIZE(remainder, leftover);
                        HDR_SET_FLAG(remainder, true, false);

                        // make new footer
                        MemHeader* remainder_foot = (MemHeader*)((char*)remainder + leftover);
                        HDR_SET_SIZE(remainder_foot, leftover);
                        HDR_SET_FLAG(remainder_foot, true, false);

                        // Set next blocks prev_free 
                        MemHeader* next_next = HDR_NEXT(remainder);
                        if (!HDR_IS_BOUNDARY(next_next))
                                HDR_SET_FLAG(next_next, HDR_IS_FREE(next_next), true);
                        else
                                next_next->size_flags |= PREV_FREE_BIT;

                        add_free_block((FreeBlock*)remainder);
                } else {
                        // take all
                        HDR_SET_SIZE(curr, total_free);

                        MemHeader* next_next = HDR_NEXT(curr);
                        if (!HDR_IS_BOUNDARY(next_next))
                                HDR_SET_FLAG(next_next, HDR_IS_FREE(next_next), false);
                        else
                                next_next->size_flags &= ~PREV_FREE_BIT;
                }

                return ptr;
        }

        void* new_ptr = taipan_realloc(NULL, 0, new_size);
        if (new_ptr != NULL) {
                memcpy(new_ptr, ptr, old_size);
                taipan_realloc(ptr, old_size, 0);
        }

        return new_ptr;
}

void* reallocate(void* pointer, size_t old_size, size_t new_size) { // should write custom mem handling logic
        vm.bytes_allocated += new_size - old_size;
        if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
                collect_garbage();
#endif // DEBUG_STRESS_GC
                if (vm.bytes_allocated > vm.next_gc)
                        collect_garbage();
        }

        if (new_size == 0) {
#ifdef USE_CUSTOM_MEMORY
                taipan_realloc(pointer, old_size, new_size);
#else
                free(pointer);
#endif // USE_CUSTOM_MEMORY
                return NULL;
        }

#ifdef USE_CUSTOM_MEMORY
        void* result = taipan_realloc(pointer, old_size, new_size);
#else
        void* result = realloc(pointer, new_size);
#endif // USE_CUSTOM_MEMORY

        if (result == NULL)
        {
                fprintf(stderr, "Realloc returned NULL");
#ifdef USE_CUSTOM_MEMORY
                memory_dump();
#endif // USE_CUSTOM_MEMORY
                exit(1);
        }
        return result;
}

static void free_object(Obj* object) {
#ifdef DEBUG_LOG_GC
        printf("%p free type %d\n", (void*)object, object->type);
#endif

        switch (object->type) {
                case OBJ_CLOSURE: {
                        ObjClosure* closure = (ObjClosure*)object;
                        FREE_ARRAY(ObjUpvalue*, closure->upvalues,
                                closure->upvalue_count);
                        FREE(ObjClosure, object);
                        break;
                }
                case OBJ_FUNCTION: {
                        ObjFunction* function = (ObjFunction*)object;
                        free_chunk(&function->chunk);
                        FREE(ObjFunction, object);
                        break;
                }
                case OBJ_NATIVE:
                        FREE(ObjNative, object);
                        break;
                case OBJ_STRING: {
                        ObjString* string = (ObjString*)object;
                        FREE_ARRAY(char, string->chars, string->length + 1);
                        FREE(ObjString, object);
                        break;
                }
                case OBJ_UPVALUE:
                        FREE(ObjUpvalue, object);
                        break;
        }
}

void mark_obj(Obj* obj) {
        if (obj == NULL) return;
        if (obj->is_marked) return;
#ifdef DEBUG_LOG_GC
        printf("%p mark ", (void*)obj);
        print_value(OBJ_VAL(obj));
        printf("\n");
#endif // DEBUG_LOG_GC

        obj->is_marked = true;

        if (vm.grey_capacity < vm.grey_count + 1) {
                size_t old_cap = vm.grey_capacity;
                vm.grey_capacity = GROW_CAPACITY(vm.grey_capacity);
                vm.grey_stack = (Obj**)taipan_realloc(vm.grey_stack, sizeof(Obj*) * old_cap, sizeof(Obj*) * vm.grey_capacity);
        }

        vm.grey_stack[vm.grey_count++] = obj;

        if (vm.grey_stack == NULL) exit(1); // somethings fucked up severely
        }

void mark_value(Value value) {
        if (IS_OBJ(value))
                mark_obj(AS_OBJ(value));
}

static void mark_arr(ValueArray* array) {
        for (int i = 0; i < array->count; i++)
                mark_value(array->values[i]);
}

static void blacken_obj(Obj* obj) {
#ifdef DEBUG_LOG_GC
        printf("%p blacken ", (void*)obj);
        print_value(OBJ_VAL(obj));
        printf("\n");
#endif // DEBUG_LOG_GC
        switch (obj->type) {
                case OBJ_CLOSURE: {
                        ObjClosure* closure = (ObjClosure*)obj;
                        mark_obj((Obj*)closure->function);
                        for (int i = 0; i < closure->upvalue_count; i++)
                                mark_obj((Obj*)closure->upvalues[i]);
                        break;
                }
                case OBJ_FUNCTION:
                        ObjFunction* function = (ObjFunction*)obj;
                        mark_obj((Obj*)function->name);
                        mark_arr(&function->chunk.constants);
                        break;
                case OBJ_UPVALUE:
                        mark_value(((ObjUpvalue*)obj)->closed);
                        break;
                case OBJ_NATIVE:
                case OBJ_STRING:
                        break;
        }
}

static void mark_roots() {
        for (Value* slot = vm.stack; slot < vm.stack_top; slot++)
                mark_value(*slot);

        for (int i = 0; i < vm.frame_count; i++)
                mark_obj((Obj*)vm.frames[i].closure);

        for (ObjUpvalue* upvalue = vm.open_upvalues;
                upvalue != NULL;
                upvalue = upvalue->next) {
                mark_obj((Obj*)upvalue);
        }

        mark_table(&vm.globals);
        mark_compiler_roots();
}

static void trace_refs() {
        while (vm.grey_count > 0) {
                Obj* object = vm.grey_stack[--vm.grey_count];
                blacken_obj(object);
        }
}

static void sweep() {
        Obj* prev = NULL;
        Obj* obj = vm.objects;
        while (obj != NULL) {
                if (obj->is_marked) {
                        obj->is_marked = false;
                        prev = obj;
                        obj = obj->next;
                } else {
                        Obj* unreached = obj;
                        obj = obj->next;
                        if (prev != NULL)
                                prev->next = obj;
                        else
                                vm.objects = obj;

                        free_object(unreached);
                }
        }
}

void collect_garbage() {
#ifdef DEBUG_LOG_GC
        printf("-- gc begin\n");
        size_t before = vm.bytes_allocated;
#endif

        mark_roots();
        trace_refs();
        table_remove_white(&vm.strings);
        sweep();

        vm.next_gc = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
        printf("-- gc end\n");
        printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
                before - vm.bytes_allocated, before, vm.bytes_allocated, vm.next_gc);
#endif
#ifdef DEBUG_MEM_DUMP
        memory_dump();
#endif
}

void free_objects() {
        Obj* object = vm.objects;
        while (object != NULL) {
                Obj* next = object->next;
                free_object(object);
                object = next;
        }

        taipan_realloc(vm.grey_stack, 0, 0);
}

#undef HDR_SET_SIZE
#undef HDR_SET_FLAG
