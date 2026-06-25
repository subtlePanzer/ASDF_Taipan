// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include <stdatomic.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "api.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "hardware.h"
#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

VM vm;

#define TP_CLOCKS_PER_SEC 1000

static void reset_stack() {
        vm.stack_top = vm.stack;
        vm.frame_count = 0;
        vm.open_upvalues = NULL;
}

static void runtime_error(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fputs("\n", stderr);

        stack_trace(&vm);

        reset_stack();
}

static void verify_native_arguments(int argc, int arity, const char* name) {
        if (argc != arity)
                runtime_error("Expected %d arguments in native function '%s', got %d instead.\n", arity, name, argc);
}

static Value clock_native(int argc, Value* argv) { // TODO: challenges 2-4 in "calls and functions", ch. 24
        verify_native_arguments(argc, 0, "clock");

        return NUMBER_VAL((double)millis() / TP_CLOCKS_PER_SEC);
}

static Value delay_native(int argc, Value* argv) {
        verify_native_arguments(argc, 1, "delay");

        if (IS_NUMBER(*argv))
                delay(AS_NUMBER(*argv));

        return NIL_VAL;
}

static Value button_down_native(int argc, Value* argv) {
        verify_native_arguments(argc, 1, "is_button_down");

#define COMPARE_BUTTON(val) do { \
        if (strcmp(str_val, #val) == 0) \
                outval = controller_get_digital(E_CONTROLLER_MASTER, E_CONTROLLER_DIGITAL_##val); \
        } while (0)

        if (argv->type == VAL_OBJ && argv->as.obj->type == OBJ_STRING) {
                const char* str_val = AS_CSTRING(*argv);
                int outval = -1;
                COMPARE_BUTTON(X);
                COMPARE_BUTTON(Y);
                COMPARE_BUTTON(A);
                COMPARE_BUTTON(B);

                COMPARE_BUTTON(UP);
                COMPARE_BUTTON(DOWN);
                COMPARE_BUTTON(LEFT);
                COMPARE_BUTTON(RIGHT);

                COMPARE_BUTTON(L1);
                COMPARE_BUTTON(L2);
                COMPARE_BUTTON(R1);
                COMPARE_BUTTON(R2);

                return NUMBER_VAL(outval);
        }
        runtime_error("Wrong argument types passed to function 'is_button_down'.\n");
#undef COMPARE_BUTTON
}

static Value stick_position_native(int argc, Value* argv) {
        verify_native_arguments(argc, 1, "get_stick_position");

        if (argv->type == VAL_NUMBER) {
                int axis = (int)argv->as.number;
                int outval = 0;
                switch (axis) {
                        case 1:
                                outval = controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_RIGHT_X);
                                break;

                        case 2:
                                outval = controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_RIGHT_Y);
                                break;

                        case 3:
                                outval = controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_LEFT_Y);
                                break;

                        case 4:
                                outval = controller_get_analog(E_CONTROLLER_MASTER, E_CONTROLLER_ANALOG_LEFT_X);
                                break;

                        default:
                                runtime_error("Axis '%d' not valid in native function 'get_stick_position'.\n", axis);
                }
                return NUMBER_VAL(outval);
        }
        runtime_error("Wrong argument types passed to function 'get_stick_position'.\n");
}

static void define_native(const char* name, NativeFn function) {
        push(OBJ_VAL(copy_string(name, (int)strlen(name))));
        push(OBJ_VAL(new_native(function)));
        table_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
        pop();
        pop();
}

void init_VM() {
        reset_stack();
        vm.objects = NULL;
        vm.bytes_allocated = 0;
        vm.next_gc = 1024 * 1024;

        vm.grey_count = 0;
        vm.grey_capacity = 0;
        vm.grey_stack = NULL;

        init_table(&vm.globals);
        init_table(&vm.strings);

        define_native("clock", clock_native);
        define_native("delay", delay_native);
        define_native("is_button_down", button_down_native);
        define_native("get_stick_position", stick_position_native);
}

void free_VM() {
        free_table(&vm.globals);
        free_table(&vm.strings);
        free_objects();
}

inline void push(Value value) {
        *vm.stack_top = value;
        vm.stack_top++;
}

inline Value pop() {
        vm.stack_top--;
        return *vm.stack_top;
}

static Value peek(int dist) {
        return vm.stack_top[-1 - dist];
}

static bool call(ObjClosure* closure, int argc) {
        if (argc != closure->function->arity) {
                runtime_error("Expected %d arguments but got %d.",
                        closure->function->arity, argc);
                return false;
        }

        if (vm.frame_count == FRAMES_MAX) {
                runtime_error("Stack overflow.");
                return false;
        }

        CallFrame* frame = &vm.frames[vm.frame_count++];
        frame->closure = closure;
        frame->ip = closure->function->chunk.code;
        frame->slots = vm.stack_top - argc - 1;
        return true;
}

static bool call_value(Value callee, int argc) {
        if (!IS_OBJ(callee)) {
                runtime_error("Can only call functions and classes.");
                return false;
        }

        if (OBJ_TYPE(callee) == OBJ_CLOSURE)
                return call(AS_CLOSURE(callee), argc);

        if (OBJ_TYPE(callee) == OBJ_NATIVE) {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argc, vm.stack_top - argc);
                vm.stack_top -= argc;
                *(vm.stack_top - 1) = result; // optimisation, tweak in place
                return true;
        }

        runtime_error("Can only call functions and classes.");
        return false;
}

static ObjUpvalue* capture_upvalue(Value* local) {
        ObjUpvalue* prev_upvalue = NULL;
        ObjUpvalue* upvalue = vm.open_upvalues;
        while (upvalue != NULL && upvalue->location > local) {
                prev_upvalue = upvalue;
                upvalue = upvalue->next;
        }

        if (upvalue != NULL && upvalue->location == local) {
                return upvalue;
        }

        ObjUpvalue* created_upvalue = new_upvalue(local);
        created_upvalue->next = upvalue;

        if (prev_upvalue == NULL) {
                vm.open_upvalues = created_upvalue;
        } else {
                prev_upvalue->next = created_upvalue;
        }

        return created_upvalue;
}

static void close_upvalues(Value* last) {
        while (vm.open_upvalues != NULL &&
                vm.open_upvalues->location >= last) {
                ObjUpvalue* upvalue = vm.open_upvalues;
                upvalue->closed = *upvalue->location;
                upvalue->location = &upvalue->closed;
                vm.open_upvalues = upvalue->next;
        }
}

static bool is_falsey(Value value) {
        return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
        ObjString* b = AS_STRING(peek(0));
        ObjString* a = AS_STRING(peek(1));

        int len = a->length + b->length;
        char* chars = ALLOCATE(char, len + 1);
        memcpy(chars, a->chars, a->length);
        memcpy(chars + a->length, b->chars, b->length);
        chars[len] = '\0';

        ObjString* result = take_string(chars, len);
        pop();
        pop();
        push(OBJ_VAL(result));
}

static InterpretResult run(void* abort_flag, void* main_handle, void* vm_clean) {
        CallFrame* frame = &vm.frames[vm.frame_count - 1];
        register uint8_t* ip = frame->ip;

        // interrupt tracking
        int cycle = 0;
        volatile atomic_bool* abort_flag_set = (atomic_bool*)abort_flag;
        volatile atomic_bool* vm_cleanup_done = (atomic_bool*)vm_clean;

#define READ_BYTE() (*ip++)
#define READ_3_BYTES() (ip += 3, \
        (uint32_t)(ip[-3] \
        | (ip[-2] << 8) \
        | (ip[-1] << 16)))
#define READ_SHORT() \
        (ip += 2, \
        (uint16_t)((ip[-2] << 8) | ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_LONG_CONSTANT() (frame->closure->function->chunk.constants.values[READ_3_BYTES()])
#define READ_STRING() AS_STRING((READ_CONSTANT()));
#define READ_LONG_STRING() AS_STRING((READ_LONG_CONSTANT()));
#define BINARY_OP(value_type, op) \
        do { \
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
                        runtime_error("Operands must be numbers."); \
                        return INTERPRET_RUNTIME_ERROR; \
                } \
                double b = AS_NUMBER(pop()); \
                double a = AS_NUMBER(pop()); \
                push(value_type(a op b)); \
        } while (false)

        for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
                printf("        ");
                for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
                        printf("[ ");
                        print_value(*slot);
                        printf(" ]");
                }
                printf("\n");
                disassemble_instruction(&frame->closure->function->chunk,
                        (int)(ip - frame->closure->function->chunk.code));
#endif

                uint8_t instruction;
                switch (instruction = READ_BYTE()) {
                        case OP_CONSTANT_LONG: {
                                Value constant = READ_LONG_CONSTANT();
                                push(constant);
                                break;
                        }
                        case OP_CONSTANT: {
                                Value constant = READ_CONSTANT();
                                push(constant);
                                break;
                        }
                        case OP_NIL:      push(NIL_VAL); break;
                        case OP_TRUE:     push(BOOL_VAL(true)); break;
                        case OP_FALSE:    push(BOOL_VAL(false)); break;
                        case OP_POP:      pop(); break;
                        case OP_POPN: {
                                int num_pops = READ_BYTE();
                                for (int i = 0; i < num_pops; i++) {
                                        pop();
                                }
                                break;
                        }
                        case OP_DUP:      push(*(vm.stack_top - 1)); break;
                        case OP_GET_LOCAL: {
                                uint8_t slot = READ_BYTE();
                                push(frame->slots[slot]);
                                break;
                        }
                        case OP_GET_LOCAL_LONG: {
                                uint32_t slot = READ_3_BYTES();
                                push(frame->slots[slot]);
                                break;
                        }
                        case OP_GET_GLOBAL: {
                                ObjString* name = READ_STRING();
                                Value value;
                                if (!table_get(&vm.globals, name, &value)) {
                                        runtime_error("Undefined variable '%s'.", name->chars);
                                        return INTERPRET_RUNTIME_ERROR;
                                }
                                push(value);
                                break;
                        }
                        case OP_GET_GLOBAL_LONG: {
                                ObjString* name = READ_LONG_STRING();
                                Value value;
                                if (!table_get(&vm.globals, name, &value)) {
                                        runtime_error("Undefined variable '%s'.", name->chars);
                                        return INTERPRET_RUNTIME_ERROR;
                                }
                                push(value);
                                break;
                        }
                        case OP_GET_MG: {
                                ObjString* name = READ_STRING();
                                Value value;
                                if (!table_get(&vm.globals, name, &value)) {
                                        runtime_error("Undefined motorgroup '%s'.", name->chars);
                                        return INTERPRET_RUNTIME_ERROR;
                                }
                                push(value);
                                break;
                        }
                        case OP_GET_MG_LONG: {
                                ObjString* name = READ_LONG_STRING();
                                Value value;
                                if (!table_get(&vm.globals, name, &value)) {
                                        runtime_error("Undefined motorgroup '%s'.", name->chars);
                                        return INTERPRET_RUNTIME_ERROR;
                                }
                                push(value);
                                break;
                        }
                        case OP_DEFINE_GLOBAL: {
                                ObjString* name = READ_STRING();
                                table_set(&vm.globals, name, peek(0));
                                pop();
                                break;
                        }
                        case OP_DEFINE_GLOBAL_LONG: {
                                ObjString* name = READ_LONG_STRING();
                                table_set(&vm.globals, name, peek(0));
                                pop();
                                break;
                        }
                        case OP_DEFINE_MG: {
                                ObjString* name = READ_STRING();
                                table_set(&vm.globals, name, peek(0));
                                pop();
                                break;
                        }
                        case OP_DEFINE_MG_LONG: {
                                ObjString* name = READ_LONG_STRING();
                                table_set(&vm.globals, name, peek(0));
                                pop();
                                break;
                        }
                        case OP_SET_LOCAL: {
                                uint8_t slot = READ_BYTE();
                                frame->slots[slot] = peek(0);
                                break;
                        }
                        case OP_SET_LOCAL_LONG: {
                                uint32_t slot = READ_3_BYTES();
                                frame->slots[slot] = peek(0);
                                break;
                        }
                        case OP_SET_GLOBAL: {
                                ObjString* name = READ_STRING();
                                if (table_set(&vm.globals, name, peek(0))) {
                                        table_delete(&vm.globals, name);
                                        runtime_error("Undefined variable '%s'.", name->chars);
                                        return INTERPRET_RUNTIME_ERROR;
                                }
                                break;
                        }
                        case OP_SET_GLOBAL_LONG: {
                                ObjString* name = READ_LONG_STRING();
                                if (table_set(&vm.globals, name, peek(0))) {
                                        table_delete(&vm.globals, name);
                                        runtime_error("Undefined variable '%s'.", name->chars);
                                        return INTERPRET_RUNTIME_ERROR;
                                }
                                break;
                        }
                        case OP_GET_UPVALUE: {
                                uint8_t slot = READ_BYTE();
                                push(*frame->closure->upvalues[slot]->location);
                                break;
                        }
                        case OP_SET_UPVALUE: {
                                uint8_t slot = READ_BYTE();
                                *frame->closure->upvalues[slot]->location = peek(0);
                                break;
                        }
                        case OP_EQ: {
                                Value b = pop();
                                Value a = pop();
                                push(BOOL_VAL(values_eq(a, b)));
                                break;
                        }
                        case OP_NEQ: {
                                Value b = pop();
                                Value a = pop();
                                push(BOOL_VAL(!values_eq(a, b)));
                                break;
                        }
                        case OP_GT:       BINARY_OP(BOOL_VAL, > ); break;
                        case OP_GT_EQ:    BINARY_OP(BOOL_VAL, >= ); break;
                        case OP_LT:       BINARY_OP(BOOL_VAL, < ); break;
                        case OP_LT_EQ:    BINARY_OP(BOOL_VAL, <= ); break;
                        case OP_ADD: { // could implicit-convert to string if one is,
                                // just have to write a tedious amount of code for it
                                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                                        concatenate();
                                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                                        double b = AS_NUMBER(pop());
                                        double a = AS_NUMBER(pop());
                                        push(NUMBER_VAL(a + b));
                                } else {
                                        runtime_error(
                                                "Operands must be two numbers or two strings.");
                                        return INTERPRET_RUNTIME_ERROR;
                                }
                                break;
                        }
                        case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
                        case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
                        case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, / ); break;
                        case OP_NOT:
                                push(BOOL_VAL(is_falsey(pop())));
                                break;
                                //                        case OP_NEGATE:   *vm.stack_top = -(*(vm.stack_top - 1)); break; // optimised to negate in place, could be push(-pop())
                        case OP_NEGATE:
                                if (!IS_NUMBER(peek(0))) {
                                        runtime_error("Operand must be a number.");
                                        return INTERPRET_RUNTIME_ERROR;
                                }
                                push(NUMBER_VAL(-AS_NUMBER(pop())));
                                break;
                        case OP_PRINT_CLI: {
                                print_value(pop());
                                printf("\n");
                                break;
                        }
                        case OP_PRINT_SC: {
                                screen_erase();
                                screen_print_value(pop());
                                break;
                        }
                        case OP_JU: {
                                uint16_t offset = READ_SHORT();
                                ip += offset;
                                break;
                        }
                        case OP_JIFPT: {
                                uint16_t offset = READ_SHORT();
                                // frame->ip += is_falsey(peek(0)) * offset; // branchless, ie. doesn't use an if to implement lox's if. Could be done with the statement instead, the compiler is smarter than me lol!
                                if (is_falsey(peek(0)))
                                        ip += offset;
                                else
                                        pop();
                                break;
                        }
                        case OP_JIFPU: {
                                uint16_t offset = READ_SHORT();
                                ip += is_falsey(pop()) * offset; // branchless
                                break;
                        }
                        case OP_JITPF: {
                                uint16_t offset = READ_SHORT();
                                if (!is_falsey(peek(0)))
                                        ip += offset;
                                else
                                        pop();
                                break;
                        }
                        case OP_JITPU: {
                                uint16_t offset = READ_SHORT();
                                ip += !is_falsey(pop()) * offset;
                                break;
                        }
                        case OP_LOOP: {
                                uint16_t offset = READ_SHORT();
                                ip -= offset;
                                break;
                        }
                        case OP_CALL: {
                                int argc = READ_BYTE();

                                frame->ip = ip;

                                if (!call_value(peek(argc), argc))
                                        return INTERPRET_RUNTIME_ERROR;
                                frame = &vm.frames[vm.frame_count - 1];
                                ip = frame->ip;
                                break;
                        }
                        case OP_CALL_LONG: { // won't need this in new language
                                int argc = READ_3_BYTES();
                                if (!call_value(peek(argc), argc))
                                        return INTERPRET_RUNTIME_ERROR;
                                frame = &vm.frames[vm.frame_count - 1];
                                break;
                        }
                        case OP_CLOSURE: {
                                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                                ObjClosure* closure = new_closure(function);
                                push(OBJ_VAL(closure));
                                for (int i = 0; i < closure->upvalue_count; i++) {
                                        uint8_t is_local = READ_BYTE();
                                        uint8_t index = READ_BYTE();
                                        if (is_local) {
                                                closure->upvalues[i] =
                                                        capture_upvalue(frame->slots + index);
                                        } else {
                                                closure->upvalues[i] = frame->closure->upvalues[index];
                                        }
                                }
                                break;
                        }
                        case OP_CLOSE_UPVALUE:
                                close_upvalues(vm.stack_top - 1);
                                pop();
                                break;
                        case rOP_DT_SPIN:
                                if (!IS_NUMBER(peek(0))) runtime_error("Drivetrain power must be a number.");
                                int power = AS_NUMBER(pop());

                                motor_move(motor_left_front, power);
                                motor_move(motor_left_mid, power);
                                motor_move(motor_left_rear, power);
                                motor_move(motor_right_front, power);
                                motor_move(motor_right_mid, power);
                                motor_move(motor_right_rear, power);
                                break;
                        case rOP_DT_TURN:
                                if (!IS_NUMBER(peek(0))) runtime_error("Drivetrain turn power must be a number.");
                                int turn_diff = 0.5 * AS_NUMBER(pop());

                                motor_move(motor_left_front, turn_diff);
                                motor_move(motor_left_mid, turn_diff);
                                motor_move(motor_left_rear, turn_diff);
                                motor_move(motor_right_front, -turn_diff);
                                motor_move(motor_right_mid, -turn_diff);
                                motor_move(motor_right_rear, -turn_diff);
                                break;
                        case rOP_MOTOR_SPIN:
                                Value mpower = pop();
                                Value port = pop();
                                if (!IS_PORT(port))
                                        runtime_error("Can only spin a port");
                                if (!IS_NUMBER(mpower))
                                        runtime_error("Motor power must be a number.");

                                motor_move(AS_PORT(port), AS_NUMBER(mpower));
                                break;
                        case rOP_MOTOR_GROUP_SPIN:
                                break; // TODO:
                        case OP_RETURN: {
                                // Exit interpreter;
                                Value result = pop();
                                close_upvalues(frame->slots);
                                vm.frame_count--;
                                if (vm.frame_count == 0) {
                                        pop();
                                        return INTERPRET_OK;
                                }

                                vm.stack_top = frame->slots;
                                push(result);
                                frame = &vm.frames[vm.frame_count - 1];
                                ip = frame->ip;
                                break;
                        }
                }

                if ((++cycle & 10) == 0 && *abort_flag_set) {
                        printf("🐍 VM: Interrupt recieved. Aborting...\n");
                        *vm_cleanup_done = true;
                        task_notify((task_t*)main_handle);
                        return INTERPRET_ABORTED;
                }
        }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_LONG_CONSTANT
#undef READ_STRING
#undef READ_LONG_STRING
#undef BINARY_OP
}

InterpretResult interpret(ObjFunction* function, void* abort_flag, void* main_handle, void* vm_clean) { // just pipe through the struct
        if (function == NULL) return INTERPRET_COMPILE_ERROR;

        push(OBJ_VAL(function));

        ObjClosure* closure = new_closure(function);
        pop();
        push(OBJ_VAL(closure));
        call(closure, 0);
        return run(abort_flag, main_handle, vm_clean);
}
