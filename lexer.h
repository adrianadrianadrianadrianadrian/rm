#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include "utils.h"

enum token_type {
    SEMICOLON = 1,
    COLON,
    IDENTIFIER,
    RIGHT_ARROW,
    LEFT_ARROW,
    COMMA,
    PIPE,
    CHAR_LITERAL,
    STR_LITERAL,
    STAR,
    AND,
    NUMERIC,
    HASH,
    DOT,
    QUESTION_MARK,
    EQ,
    BANG,
    MOD,
    DIV,
    PLUS,
    MINUS,
    
    // keywords
    FN_KEYWORD,
    ENUM_KEYWORD,
    STRUCT_KEYWORD,
    IF_KEYWORD,
    WHILE_KEYWORD,
    RETURN_KEYWORD,
    BOOLEAN_TRUE_KEYWORD,
    BOOLEAN_FALSE_KEYWORD,
    ELSE_KEYWORD,
    BREAK_KEYWORD,
    MUTABLE_KEYWORD,
    NULL_KEYWORD,
    SWITCH_KEYWORD,
    CASE_KEYWORD,
    LET_KEYWORD,

    // parens
    OPEN_ROUND_PAREN,
    CLOSE_ROUND_PAREN,
    OPEN_CURLY_PAREN,
    CLOSE_CURLY_PAREN,
    OPEN_SQUARE_PAREN,
    CLOSE_SQUARE_PAREN,
};

struct token_metadata {
    unsigned int row;
    unsigned int col;
    unsigned int length;
    char *file_name;
};

typedef struct token {
    enum token_type token_type;
    union {
        struct list_char *identifier;
        double numeric;
    };
    struct token_metadata metadata;
} token;

typedef struct positional_char {
    char value;
    int row;
    int col;
    char *file_name;
} positional_char;

struct_list(positional_char);

struct file_buffer {
    struct positional_char *data;
    size_t current_position;
    size_t size;
};

struct_list(token);

struct token_buffer {
    struct list_token tokens;
    size_t current_position;
    size_t size;
    struct file_buffer source;
};

struct token_buffer create_token_buffer(FILE *fstream, char *file_name);
void seek_back_token(struct token_buffer *s, size_t amount);
int get_token(struct token_buffer *s, struct token *out);
int get_token_type(struct token_buffer *s, struct token *out, enum token_type ty);
struct token_metadata *get_token_metadata(struct token_buffer *toks, size_t position);

#endif
