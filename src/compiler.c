// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "memory.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif // DEBUG_PRINT_CODE

typedef struct {
        Token prev;
        Token curr;

        bool had_err;
        bool panic_mode;
} Parser;

typedef enum {
        PREC_NONE,
        PREC_ASSIGNMENT, // =
        PREC_OR, // or
        PREC_AND, // and
        PREC_EQUALITY, // != ==
        PREC_COMPARISON, // < > <= =>
        PREC_TERM, // + -
        PREC_FACTOR, // * / 
        PREC_UNARY, // - !
        PREC_CALL, // () .
        PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool can_assign);

typedef struct {
        ParseFn prefix;
        ParseFn infix;
        Precedence precedence;
} ParseRule;

typedef struct {
        Token name;
        int depth;
        bool is_captured;
} Local;

typedef struct {
        uint8_t index;
        bool is_local;
} Upvalue;

typedef enum {
        TYPE_FUNCTION,
        TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
        struct Compiler* enclosing;
        ObjFunction* function;
        FunctionType type;
        Local* locals;
        int local_count;
        Upvalue upvalues[UINT8_COUNT];
        int local_capacity;
        int scope_depth;
} Compiler;

Parser parser;
Compiler* current = NULL;

static Chunk* current_chunk() {
        return &current->function->chunk;
}

static void error_at(Token* token, const char* msg) {
        if (parser.panic_mode) return;
        parser.panic_mode = true;
        fprintf(stderr, "[line %d] Error", token->line);

        if (token->type == TOKEN_EOF)
                fprintf(stderr, " at end");
        else if (token->type == TOKEN_ERROR)
                ;
        else
                fprintf(stderr, " at '%.*s'", token->length, token->start);

        fprintf(stderr, ": %s\n", msg);
        parser.had_err = true;
}

static void error_at_curr(const char* msg) {
        error_at(&parser.curr, msg);
}

static void error(const char* msg) {
        error_at(&parser.prev, msg);
}

static void advance() {
        parser.prev = parser.curr;

        for (;;) {
                parser.curr = scan_token();
                if (parser.curr.type != TOKEN_ERROR) break;

                error_at_curr(parser.curr.start);
        }
}

static void consume(TokenType type, const char* msg) {
        if (parser.curr.type == type) {
                advance();
                return;
        }

        error_at_curr(msg);
}

static bool check(TokenType type) {
        return parser.curr.type == type;
}

static bool match(TokenType type) {
        if (!check(type)) return false;
        advance();
        return true;
}

static void emit_byte(uint8_t byte) {
        write_chunk(current_chunk(), byte, parser.prev.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
        emit_byte(byte1);
        emit_byte(byte2);
}

static void emit_4_bytes(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
        emit_byte(byte1);
        emit_byte(byte2);
        emit_byte(byte3);
        emit_byte(byte4);
}

static void emit_loop(int loop_start) {
        emit_byte(OP_LOOP);

        int offset = current_chunk()->count - loop_start + 2;
        if (offset > UINT16_MAX) error("Loop body too large.");

        emit_byte((offset >> 8) & 0xff);
        emit_byte(offset & 0xff);
}

static int emit_jump(uint8_t instruction) { // swap with a 2 byte jump instruction and a 4 byte long jump instruction
        emit_byte(instruction);
        emit_byte(0xff);
        emit_byte(0xff);
        return current_chunk()->count - 2;
}

static void emit_return() {
        emit_byte(OP_NIL);
        emit_byte(OP_RETURN);
}

static int make_constant(Value value) {
        int constant = add_constant(current_chunk(), value);
        if (constant > UINT24_MAX) {
                error("Too many constants in one chunk.");
                return 0;
        }

        return constant;
}

static void emit_long_constant(int constant) {
        emit_4_bytes(OP_CONSTANT_LONG,
                (uint8_t)(constant & 0xff),
                (uint8_t)((constant >> 8) & 0xff),
                (uint8_t)((constant >> 16) & 0xff));
}

static void emit_constant(Value value) {
        int constant = make_constant(value);

        if (constant > 256) {
                emit_long_constant(constant);
        } else {
                emit_bytes(OP_CONSTANT, (uint8_t)constant);
        }
}

static void patch_jump(int offset) {
        // -2 for jump offset
        int jump = current_chunk()->count - offset - 2;

        if (jump > UINT16_MAX)
                error("Too much code to jump over.");

        current_chunk()->code[offset] = (jump >> 8) & 0xff;
        current_chunk()->code[offset + 1] = jump & 0xff;
}

static void add_null_local() {
        if (current->local_count == UINT24_COUNT) {
                error("Too many local variables in function.");
                return;
        }

        if (current->local_capacity < current->local_count + 1) {
                int old_cap = current->local_capacity;
                current->local_capacity = GROW_CAPACITY(old_cap);
                current->locals = GROW_ARRAY(Local, current->locals, old_cap,
                        current->local_capacity);
        }

        Local* local = &current->locals[current->local_count++];
        local->depth = 0;
        local->is_captured = false;
        local->name.start = "";
        local->name.length = 0;
}

static void init_compiler(Compiler* compiler, FunctionType type) {
        compiler->enclosing = current;
        compiler->function = NULL;
        compiler->type = type;
        compiler->local_count = 0;
        compiler->local_capacity = 0;
        compiler->locals = NULL;
        compiler->scope_depth = 0;
        compiler->local_capacity = 0;
        compiler->function = new_function();
        current = compiler;

        if (type != TYPE_SCRIPT)
                current->function->name = copy_string(parser.prev.start,
                        parser.prev.length);

        add_null_local();
}

static ObjFunction* end_compiler() {
        emit_return();
        ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
        if (!parser.had_err)
                disassemble_chunk(current_chunk(), function->name != NULL
                        ? function->name->chars : "<script>");
#endif // DEBUG_PRINT_CODE

        current = current->enclosing;
        return function;
}

static void begin_scope() {
        current->scope_depth++;
}

static void end_scope() {
        current->scope_depth--;

        while (current->local_count > 0 &&
                current->locals[current->local_count - 1].depth >
                current->scope_depth) {

                if (current->locals[current->local_count - 1].is_captured) {
                        emit_byte(OP_CLOSE_UPVALUE);
                } else {
                        emit_byte(OP_POP);
                }
                current->local_count--;
        }
}

static void expression();
static void statement();
static void declaration();
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);

static int identifier_constant(Token* name) {
        ObjString* string = copy_string(name->start, name->length);

        for (int i = 0; i < current_chunk()->constants.count; i++) {
                Value v = current_chunk()->constants.values[i];
                if (IS_STRING(v) && AS_STRING(v) == string)
                        return i;
        }

        push(OBJ_VAL(string));

        int constant = make_constant(OBJ_VAL(string));

        pop();

        return constant;
}

static bool identifiers_equal(Token* a, Token* b) {
        if (a->length != b->length) return false;
        return memcmp(a->start, b->start, a->length) == 0; // optimisation: check hashes (which haven't been calc'd yet)
}

static int resolve_local(Compiler* compiler, Token* name) {
        for (int i = compiler->local_count - 1; i >= 0; i--) {
                Local* local = &compiler->locals[i];
                if (identifiers_equal(name, &local->name)) {
                        if (local->depth == -1)
                                error("Can't read local variable in its own "
                                        "initialiser.");
                        return i;
                }
        }

        return -1;
}

static int add_upvalue(Compiler* compiler, uint8_t index,
        bool isLocal) {
        int upvalue_count = compiler->function->upvalue_count;

        for (int i = 0; i < upvalue_count; i++) {
                Upvalue* upvalue = &compiler->upvalues[i];
                if (upvalue->index == index && upvalue->is_local == isLocal) {
                        return i;
                }
        }

        if (upvalue_count == UINT8_COUNT) {
                error("Too many closure variables in function.");
                return 0;
        }

        compiler->upvalues[upvalue_count].is_local = isLocal;
        compiler->upvalues[upvalue_count].index = index;
        return compiler->function->upvalue_count++;
}

static int resolve_upvalue(Compiler* compiler, Token* name) {
        if (compiler->enclosing == NULL) return -1;

        int local = resolve_local(compiler->enclosing, name);
        if (local != -1) {
                compiler->enclosing->locals[local].is_captured = true;
                return add_upvalue(compiler, (uint8_t)local, true);
        }

        int upvalue = resolve_upvalue(compiler->enclosing, name);
        if (upvalue != -1) {
                return add_upvalue(compiler, (uint8_t)upvalue, false);
        }

        return -1;
}

static void add_local(Token name) {
        if (current->local_count == UINT24_COUNT) {
                error("Too many local variables in function.");
                return;
        }

        if (current->local_capacity < current->local_count + 1) {
                int old_cap = current->local_capacity;
                current->local_capacity = GROW_CAPACITY(old_cap);
                current->locals = GROW_ARRAY(Local, current->locals, old_cap,
                        current->local_capacity);
        }

        Local* local = &current->locals[current->local_count++];
        local->name = name;
        local->depth = -1;
        local->is_captured = false;
}

static void declare_var() {
        if (current->scope_depth == 0) return;

        Token* name = &parser.prev;
        for (int i = current->local_count - 1; i >= 0; i--) {
                Local* local = &current->locals[i];
                if (local->depth != -1 && local->depth < current->scope_depth)
                        break;

                if (identifiers_equal(name, &local->name))
                        error("Already a variable with this name in this scope.");
        }

        add_local(*name);
}

static int parse_var(const char* error_msg) {
        consume(TOKEN_IDENTIFIER, error_msg);

        declare_var();
        if (current->scope_depth > 0) return 0;

        return identifier_constant(&parser.prev);
}

static void mark_init() {
        if (current->scope_depth == 0) return;
        current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void define_var(int global) {
        if (current->scope_depth > 0) {
                mark_init();
                return;
        }

        if (global > UINT8_MAX)
                emit_4_bytes(OP_DEFINE_GLOBAL_LONG,
                        global & 0xff,
                        (global >> 8) & 0xff,
                        (global >> 16) & 0xff);
        else
                emit_bytes(OP_DEFINE_GLOBAL, global);
}

static int arg_list() {
        int argc = 0; // make long call?
        if (!check(TOKEN_RIGHT_PAREN)) {
                do {
                        expression();
                        // if (argc == 255) error("Can't have more than 255 arguments.");
                        if (argc == UINT24_MAX) error("Can't have more than 16"
                                "million arguments.");
                        argc++;
                } while (match(TOKEN_COMMA));
        }
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
        return argc;
}

static void and_(bool can_assign) { // could use custom instruction that implicitly doesn't pop and swap the old one for another which does
        int end_jump = emit_jump(OP_JIFPT);

        parse_precedence(PREC_AND);

        patch_jump(end_jump);
}

static void binary(bool can_assign) {
        TokenType op_type = parser.prev.type;
        ParseRule* rule = get_rule(op_type);
        parse_precedence((Precedence)(rule->precedence + 1));

        switch (op_type) {
                case TOKEN_BANG_EQ: emit_byte(OP_NEQ);      break;
                case TOKEN_EQ_EQ:   emit_byte(OP_EQ);       break;
                case TOKEN_GT:      emit_byte(OP_GT);       break;
                case TOKEN_GT_EQ:   emit_byte(OP_GT_EQ);    break;
                case TOKEN_LT:      emit_byte(OP_LT);       break;
                case TOKEN_LT_EQ:   emit_byte(OP_LT_EQ);    break;
                case TOKEN_PLUS:    emit_byte(OP_ADD);      break;
                case TOKEN_MINUS:   emit_byte(OP_SUBTRACT); break;
                case TOKEN_STAR:    emit_byte(OP_MULTIPLY); break;
                case TOKEN_SLASH:   emit_byte(OP_DIVIDE);   break;

                default: return;
        }
}

static void call(bool can_assign) {
        int argc = arg_list();
        if (argc <= 255)
                emit_bytes(OP_CALL, (uint8_t)argc);
        else
                emit_4_bytes(OP_CALL_LONG, (uint8_t)(argc & 0xff),
                        (uint8_t)((argc >> 8) & 0xff),
                        (uint8_t)((argc >> 16) & 0xff));
}

static void literal(bool can_assign) { // can swap for 3 individual functions for optimisation (-1 switch)
        switch (parser.prev.type) {
                case TOKEN_FALSE: emit_byte(OP_FALSE); break;
                case TOKEN_NIL:   emit_byte(OP_NIL); break;
                case TOKEN_TRUE:  emit_byte(OP_TRUE); break;
                default: return; // unreachable
        }
}

static void grouping(bool can_assign) {
        expression();
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool can_assign) {
        double value = strtod(parser.prev.start, NULL);
        emit_constant(NUMBER_VAL(value));
}

static void or_(bool can_assign) {
        int end_jump = emit_jump(OP_JITPF);

        parse_precedence(PREC_OR);
        patch_jump(end_jump);
}

static void string(bool can_assign) { // translate escape sequences here
        emit_constant(OBJ_VAL(copy_string(parser.prev.start + 1,
                parser.prev.length - 2)));
}

static void named_var(Token name, bool can_assign) {
        uint8_t get_op, set_op, get_op_long, set_op_long;
        int arg = resolve_local(current, &name);
        if (arg != -1) {
                get_op = OP_GET_LOCAL;
                set_op = OP_SET_LOCAL;
                get_op_long = OP_GET_LOCAL_LONG;
                set_op_long = OP_SET_LOCAL_LONG;
        } else if ((arg = resolve_upvalue(current, &name)) != -1) {
                get_op = OP_GET_UPVALUE;
                set_op = OP_SET_UPVALUE;
        } else {
                arg = identifier_constant(&name);
                get_op = OP_GET_GLOBAL;
                set_op = OP_SET_GLOBAL;
                get_op_long = OP_GET_GLOBAL_LONG;
                set_op_long = OP_SET_GLOBAL_LONG;
        }

        if (can_assign && match(TOKEN_EQ)) {
                expression();
                if (arg > UINT8_MAX) {
                        emit_4_bytes(set_op_long,
                                (uint8_t)(arg & 0xff),
                                (uint8_t)((arg >> 8) & 0xff),
                                (uint8_t)((arg >> 16) & 0xff));
                } else {
                        emit_bytes(set_op, (uint8_t)arg);
                }
        } else {
                if (arg > UINT8_MAX) {
                        emit_4_bytes(get_op_long,
                                (uint8_t)(arg & 0xff),
                                (uint8_t)((arg >> 8) & 0xff),
                                (uint8_t)((arg >> 16) & 0xff));
                } else {
                        emit_bytes(get_op, (uint8_t)arg);
                }
        }
}

static void variable(bool can_assign) {
        named_var(parser.prev, can_assign);
}

static void unary(bool can_assign) {
        TokenType op = parser.prev.type;

        parse_precedence(PREC_UNARY);

        switch (op) {
                case TOKEN_BANG: emit_byte(OP_NOT); break;
                case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
                default: break; // unreachable
        }
}

ParseRule rules[] = {
        [TOKEN_LEFT_PAREN] = {grouping, call,   PREC_CALL},
        [TOKEN_RIGHT_PAREN] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_LEFT_BRACE] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_RIGHT_BRACE] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_COMMA] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_PERIOD] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_MINUS] = {unary,    binary, PREC_TERM},
        [TOKEN_PLUS] = {NULL,     binary, PREC_TERM},
        [TOKEN_SEMICOLON] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_SLASH] = {NULL,     binary, PREC_FACTOR},
        [TOKEN_STAR] = {NULL,     binary, PREC_FACTOR},
        [TOKEN_BANG] = {unary,     NULL,   PREC_NONE},
        [TOKEN_BANG_EQ] = {NULL,     binary,   PREC_EQUALITY},
        [TOKEN_EQ] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_EQ_EQ] = {NULL,     binary,   PREC_EQUALITY},
        [TOKEN_GT] = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_GT_EQ] = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_LT] = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_LT_EQ] = {NULL,     binary,   PREC_COMPARISON},
        [TOKEN_IDENTIFIER] = {variable,     NULL,   PREC_NONE},
        [TOKEN_STRING] = {string,     NULL,   PREC_NONE},
        [TOKEN_NUMBER] = {number,   NULL,   PREC_NONE},
        [TOKEN_AND] = {NULL,     and_,   PREC_AND},
        [TOKEN_CLASS] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_ELSE] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_FALSE] = {literal,     NULL,   PREC_NONE},
        [TOKEN_FOR] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_FUN] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_IF] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_SWITCH] = {NULL, NULL, PREC_NONE},
        [TOKEN_CASE] = {NULL, NULL, PREC_NONE},
        [TOKEN_DEFAULT] = {NULL, NULL, PREC_NONE},
        [TOKEN_NIL] = {literal,     NULL,   PREC_NONE},
        [TOKEN_OR] = {NULL,     or_,   PREC_OR},
        [TOKEN_PRINT_CLI] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_PRINT_SC] = {NULL, NULL, PREC_NONE},
        [TOKEN_RETURN] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_SUPER] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_THIS] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_TRUE] = {literal,     NULL,   PREC_NONE},
        [TOKEN_VAR] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_WHILE] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_ERROR] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_EOF] = {NULL,     NULL,   PREC_NONE},
        [TOKEN_SET_DT] = {NULL, NULL, PREC_NONE},
        [TOKEN_SET_MOTOR] = {NULL, NULL, PREC_NONE},
        [TOKEN_SET_MOTORGROUP] = {NULL, NULL, PREC_NONE},
        [TOKEN_MOTOR] = {NULL, NULL, PREC_NONE},
        [TOKEN_MOTORGROUP] = {NULL, NULL, PREC_NONE},
};

static void parse_precedence(Precedence precedence) {
        advance();

        ParseFn prefix_rule = get_rule(parser.prev.type)->prefix;
        if (prefix_rule == NULL) {
                error("Expect expression.");
                return;
        }

        bool can_assign = precedence <= PREC_ASSIGNMENT;
        prefix_rule(can_assign);

        while (precedence <= get_rule(parser.curr.type)->precedence) {
                advance();
                ParseFn infix_rule = get_rule(parser.prev.type)->infix;
                infix_rule(can_assign);
        }

        if (can_assign && match(TOKEN_EQ))
                error("Invalid assignment target.");
}

static ParseRule* get_rule(TokenType type) {
        return &rules[type];
}

static void expression() {
        parse_precedence(PREC_ASSIGNMENT);
}

static void block() {
        while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
                declaration();
        }

        consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
        Compiler compiler;
        init_compiler(&compiler, type);
        begin_scope();

        consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
        if (!check(TOKEN_RIGHT_PAREN)) {
                do {
                        current->function->arity++;
                        if (current->function->arity > 255)
                                error_at_curr("Can't have more than 255 parameters.");

                        uint8_t constant = parse_var("Expect parameter name.");
                        define_var(constant);
                } while (match(TOKEN_COMMA));
        }
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after function parameters.");
        consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
        block();

        ObjFunction* function = end_compiler();
        emit_bytes(OP_CLOSURE, make_constant(OBJ_VAL(function)));

        for (int i = 0; i < function->upvalue_count; i++) {
                emit_byte(compiler.upvalues[i].is_local ? 1 : 0);
                emit_byte(compiler.upvalues[i].index);
        }
}

static void fun_declaration() {
        uint8_t global = parse_var("Expect function name.");
        mark_init();
        function(TYPE_FUNCTION);
        define_var(global);
}

static void var_declaration() {
        int global = parse_var("Expect variable name.");

        if (match(TOKEN_EQ))
                expression();
        else
                emit_byte(OP_NIL);

        consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");

        define_var(global);
}

static void expression_statement() {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
        emit_byte(OP_POP);
}

static void for_statement() {
        begin_scope();
        consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

        if (match(TOKEN_SEMICOLON));
        else if (match(TOKEN_VAR))
                var_declaration();
        else
                expression_statement();

        int loop_start = current_chunk()->count;
        int exit_jump = -1;
        if (!match(TOKEN_SEMICOLON)) {
                expression();
                consume(TOKEN_SEMICOLON, "Expect ';' after loop condition");

                exit_jump = emit_jump(OP_JIFPT);
        }

        if (!match(TOKEN_RIGHT_PAREN)) {
                int body_jump = emit_jump(OP_JU);
                int increment_start = current_chunk()->count;
                expression();
                emit_byte(OP_POP);
                consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses");

                emit_loop(loop_start);
                loop_start = increment_start;
                patch_jump(body_jump);
        }

        statement();
        emit_loop(loop_start);

        if (exit_jump != -1) {
                patch_jump(exit_jump);
                emit_byte(OP_POP);
        }

        end_scope();
}

static void switch_statement() { // Check for OP_POP
        consume(TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
        expression();
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after switch expression.");

        consume(TOKEN_LEFT_BRACE, "ExpecFt '{' for start of switch block.");

        // list of case statements
        int last_jump = -1; // jump to the next end jump which repeats until it reaches the end of the default block
        for (;;) {
                if (!match(TOKEN_CASE))
                        break;

                emit_byte(OP_DUP);
                expression();

                emit_byte(OP_EQ);
                int next_case_jump = emit_jump(OP_JIFPU);

                consume(TOKEN_COLON, "Expect ':' after case expression.");

                statement();

                if (last_jump != -1)
                        patch_jump(last_jump);

                last_jump = emit_jump(OP_JU);

                patch_jump(next_case_jump);
        }

        // default case
        if (match(TOKEN_DEFAULT)) {
                consume(TOKEN_COLON, "Expect ':' after 'default'.");

                statement();
        }

        if (last_jump != -1)
                patch_jump(last_jump);

        emit_byte(OP_POP);

        consume(TOKEN_RIGHT_BRACE, "Expect '}' for end of switch block.");
}

static void if_statement() {
        consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
        expression();
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

        int then_jump = emit_jump(OP_JIFPU);
        statement();

        int else_jump = emit_jump(OP_JU);

        patch_jump(then_jump);

        if (match(TOKEN_ELSE)) statement();
        patch_jump(else_jump);
}

static void print_cli_statement() {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after value.");
        emit_byte(OP_PRINT_CLI);
}

static void print_sc_statement() {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after value.");
        emit_byte(OP_PRINT_SC);
}

static void return_statement() {
        if (current->type == TYPE_SCRIPT)
                /* "Allowing return at the top level isn’t the worst idea in
                the world. It would give you a natural way to terminate a
                script early. You could maybe even use a returned number to
                indicate the process’s exit code." */
                error("Can't return from top-level code.");


        if (match(TOKEN_SEMICOLON)) {
                emit_return();
        } else {
                expression();
                consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
                emit_byte(OP_RETURN);
        }
}

static void while_statement() {
        int loop_start = current_chunk()->count;
        consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
        expression();
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

        int exit_jump = emit_jump(OP_JIFPU);
        statement();
        emit_loop(loop_start);

        patch_jump(exit_jump);
        emit_byte(OP_POP);
}

static void synchronise() {
        parser.panic_mode = false;

        while (parser.curr.type != TOKEN_EOF) {
                switch (parser.curr.type) {
                        case TOKEN_CLASS:
                        case TOKEN_FUN:
                        case TOKEN_VAR:
                        case TOKEN_FOR:
                        case TOKEN_IF:
                        case TOKEN_WHILE:
                        case TOKEN_SWITCH:
                        case TOKEN_PRINT_CLI:
                        case TOKEN_PRINT_SC:
                        case TOKEN_RETURN:
                                return;

                        default:
                                ; // nothing
                }

                advance();
        }
}

static void motor_decl() {
        int global = parse_var("Expect motor name.");

        if (!check(TOKEN_SEMICOLON))
                expression();
        else
                emit_byte(OP_NIL);

        consume(TOKEN_SEMICOLON, "Expect ';' after motor declaration");

        define_var(global);
}

static void motorgroup_decl() {
        int global = parse_var("Expect motorgroup name.");

        if (check(TOKEN_SEMICOLON))
                emit_byte(OP_NIL);
        else
                do {
                        expression();
                } while (!check(TOKEN_SEMICOLON));

        consume(TOKEN_SEMICOLON, "Expect ';' after motorgroup declaration");

        define_var(global);
}

static void declaration() {
        if (match(TOKEN_FUN))
                fun_declaration();
        else if (match(TOKEN_MOTOR))
                motor_decl();
        else if (match(TOKEN_MOTORGROUP))
                motorgroup_decl();
        else if (match(TOKEN_VAR))
                var_declaration();
        else
                statement();

        if (parser.panic_mode) synchronise();
}

static void dt_set_statement() { // TODO: add a version for turning ie. different inputs on each side
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after value.");
        emit_byte(rOP_DT_SPIN);
}

static void motor_set_statement() {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after port.");
        emit_byte(rOP_MOTOR_SPIN);
}

static void motorgroup_set_statement() {
        // pull motorgroup name
        consume(TOKEN_SEMICOLON, "Expect ';' after motorgroup name.");
        emit_byte(rOP_MOTOR_GROUP_SPIN);
}

static void statement() {
        if (match(TOKEN_PRINT_CLI)) {
                print_cli_statement();
        } else if (match(TOKEN_PRINT_SC)) {
                print_sc_statement();
        } else if (match(TOKEN_SET_DT)) {
                dt_set_statement();
        } else if (match(TOKEN_SET_MOTOR)) {
                motor_set_statement();
        } else if (match(TOKEN_SET_MOTORGROUP)) {
                motorgroup_set_statement();
        } else if (match(TOKEN_FOR)) {
                for_statement();
        } else if (match(TOKEN_IF)) {
                if_statement();
        } else if (match(TOKEN_RETURN)) {
                return_statement();
        } else if (match(TOKEN_WHILE)) {
                while_statement();
        } else if (match(TOKEN_SWITCH)) {
                switch_statement();
        } else if (match(TOKEN_LEFT_BRACE)) {
                begin_scope();
                block();
                end_scope();
        } else {
                expression_statement();
        }
}

ObjFunction* compile(const char* src) {
        init_scanner(src);
        Compiler compiler;
        init_compiler(&compiler, TYPE_SCRIPT);

        parser.had_err = false;
        parser.panic_mode = false;

        advance();

        while (!match(TOKEN_EOF)) {
                declaration();
        }

        ObjFunction* function = end_compiler();
        return parser.had_err ? NULL : function;
}

void mark_compiler_roots() {
        Compiler* compiler = current;
        while (compiler != NULL) {
                mark_obj((Obj*)compiler->function);
                compiler = compiler->enclosing;
        }
}
