// -----------------------------------------------------------------------------
// ASDF 2027 'Taipan' language - (c) 2026 Riley Lorenz  & ASDF Robotics
// -----------------------------------------------------------------------------

#ifndef tp_scanner_h
#define tp_scanner_h

typedef enum {

        // Single-character tokens
        TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
        TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
        TOKEN_COMMA, TOKEN_PERIOD, TOKEN_MINUS, TOKEN_PLUS,
        TOKEN_SEMICOLON, TOKEN_COLON, TOKEN_SLASH, TOKEN_STAR,

        // One or two character tokens
        TOKEN_BANG, TOKEN_BANG_EQ,
        TOKEN_EQ, TOKEN_EQ_EQ,
        TOKEN_GT, TOKEN_GT_EQ,
        TOKEN_LT, TOKEN_LT_EQ,

        // Literals
        TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

        // Keywords
        TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
        TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
        TOKEN_PRINT_CLI, TOKEN_PRINT_SC, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
        TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,
        TOKEN_SWITCH, TOKEN_CASE, TOKEN_DEFAULT,

        TOKEN_SET_DT, TOKEN_SET_MOTOR, TOKEN_SET_MOTORGROUP, TOKEN_MOTOR, TOKEN_MOTORGROUP,

        TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
        TokenType type;
        const char* start;
        int length;
        int line;
} Token;

void init_scanner(const char* src);
Token scan_token();

#endif // tp_scanner_h
