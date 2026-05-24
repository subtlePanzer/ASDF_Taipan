#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
        const char* start;
        const char* curr;
        int line;
} Scanner;

Scanner scanner;

void init_scanner(const char* src) {
        scanner.start = src;
        scanner.curr = src;
        scanner.line = 1;
}

static bool is_alpha(char c) {
        return (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                c == '_';
}

static bool is_digit(char c) {
        return c >= '0' && c <= '9';
}

static bool is_at_end() {
        return *scanner.curr == '\0';
}

static char advance() {
        scanner.curr++;
        return scanner.curr[-1];
}

static char peek() {
        return *scanner.curr;
}

static char peek_next() {
        if (is_at_end()) return '\0';
        return scanner.curr[1];
}

static bool match(char expected) {
        if (is_at_end()) return false;
        if (*scanner.curr != expected) return false;
        scanner.curr++;
        return true;
}

static Token make_token(TokenType type) {
        Token token;
        token.type = type;
        token.start = scanner.start;
        token.length = (scanner.curr - token.start);
        token.line = scanner.line;
        return token;
}

static Token error_token(const char* msg) {
        Token token;
        token.type = TOKEN_ERROR;
        token.start = msg;
        token.length = (int)strlen(msg);
        token.line = scanner.line;
        return token;
}

static void skip_ws() {
        for (;;) {
                char c = peek();

                switch (c) {
                        case ' ':
                        case '\r':
                        case '\t':
                                advance();
                                break;
                        case '\n':
                                scanner.line++;
                                advance();
                                break;
                        case '/':
                                if (peek_next() == '/')
                                        while (peek() != '\n' && !is_at_end()) advance();
                                else
                                        return;
                                break;
                        default:
                                return;
                }
        }
}

static TokenType check_keyword(int start, int len, const char* rest, TokenType type) {
        if (scanner.curr - scanner.start == start + len
                && memcmp(scanner.start + start, rest, len) == 0)
                return type;

        return TOKEN_IDENTIFIER;
}

static bool is_keyword(int start, int len, const char* rest) {
        return scanner.curr - scanner.start == start + len && (memcmp(scanner.start + start, rest, len) == 0);
}

static TokenType identifier_type() {
        switch (scanner.start[0]) {
                case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
                case 'c':
                        if (scanner.curr - scanner.start > 1) {
                                switch (scanner.start[1]) {
                                        case 'l': return check_keyword(2, 3, "ass", TOKEN_CLASS);
                                        case 'a': return check_keyword(2, 2, "se", TOKEN_CASE);
                                }
                        }
                case 'd': return check_keyword(1, 6, "efault", TOKEN_DEFAULT);
                case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
                case 'f':
                        if (scanner.curr - scanner.start > 1) {
                                switch (scanner.start[1]) {
                                        case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
                                        case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
                                        case 'u': return check_keyword(2, 1, "n", TOKEN_FUN);
                                }
                        }
                        break;
                case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
                case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
                case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
                case 'p': {
                        if (is_keyword(1, 8, "rint_cli")) return TOKEN_PRINT_CLI;
                        if (is_keyword(1, 7, "rint_sc")) return TOKEN_PRINT_SC;
                        return TOKEN_IDENTIFIER;
                }
                case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
                case 's':
                        if (scanner.curr - scanner.start > 1) {
                                switch (scanner.start[1]) {
                                        case 'u': return check_keyword(2, 3, "per", TOKEN_SUPER);
                                        case 'w': return check_keyword(2, 4, "itch", TOKEN_SWITCH);
                                }
                        }
                case 't':
                        if (scanner.curr - scanner.start > 1) {
                                switch (scanner.start[1]) {
                                        case 'h': return check_keyword(2, 2, "is", TOKEN_THIS);
                                        case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
                                }
                        }
                        break;
                case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
                case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
        }

        return TOKEN_IDENTIFIER;
}

static Token identifier() {
        while (is_alpha(peek()) || is_digit(peek())) advance();
        return make_token(identifier_type());
}

static Token number() {
        while (is_digit(peek())) advance();

        if (peek() == '.' && is_digit(peek_next())) {
                advance();
                while (is_digit(peek())) advance();
        }

        return make_token(TOKEN_NUMBER);
}

static Token string() {
        while (peek() != '"' && !is_at_end()) {
                if (peek() == '\n') scanner.line++;
                advance();
        }

        if (is_at_end())
                return error_token("Unterminated string.");

        advance(); // closing "
        return make_token(TOKEN_STRING);
}

Token scan_token() {
        skip_ws();
        scanner.start = scanner.curr;

        if (is_at_end()) return make_token(TOKEN_EOF);

        char c = advance();
        if (is_alpha(c)) return identifier();
        if (is_digit(c)) return number();

        switch (c) {
                case '(': return make_token(TOKEN_LEFT_PAREN);
                case ')': return make_token(TOKEN_RIGHT_PAREN);
                case '{': return make_token(TOKEN_LEFT_BRACE);
                case '}': return make_token(TOKEN_RIGHT_BRACE);
                case ';': return make_token(TOKEN_SEMICOLON);
                case ':': return make_token(TOKEN_COLON);
                case ',': return make_token(TOKEN_COMMA);
                case '.': return make_token(TOKEN_PERIOD);
                case '-': return make_token(TOKEN_MINUS);
                case '+': return make_token(TOKEN_PLUS);
                case '/': return make_token(TOKEN_SLASH);
                case '*': return make_token(TOKEN_STAR);
                case '!': return make_token(match('=') ? TOKEN_BANG_EQ : TOKEN_BANG);
                case '=': return make_token(match('=') ? TOKEN_EQ_EQ : TOKEN_EQ);
                case '<': return make_token(match('=') ? TOKEN_LT_EQ : TOKEN_LT);
                case '>': return make_token(match('=') ? TOKEN_GT_EQ : TOKEN_GT);
                case '"': return string();
        }

        return error_token("Unexpected character.");
}
