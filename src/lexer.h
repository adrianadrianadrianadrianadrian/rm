#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include "../lib/collections.h"

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
    C_LITERAL,
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

    // keywords           djb2_hash value
    FN_KEYWORD            = 5863385L,
    ENUM_KEYWORD          = 6385194298L,
    STRUCT_KEYWORD        = 6954031505834L,
    IF_KEYWORD            = 5863476L,
    WHILE_KEYWORD         = 210732529790L,
    RETURN_KEYWORD        = 6953974653989L,
    BOOLEAN_TRUE_KEYWORD  = 6385737701L,
    BOOLEAN_FALSE_KEYWORD = 210712121072L,
    ELSE_KEYWORD          = 6385192046L,
    BREAK_KEYWORD         = 210707980106L,
    MUTABLE_KEYWORD       = 193499675L,
    NULL_KEYWORD          = 6385525056L,
    SWITCH_KEYWORD        = 6954034739063L,
    CASE_KEYWORD          = 6385108193L,
    LET_KEYWORD           = 193498058L,

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
};

struct token_buffer create_token_buffer(FILE *fstream, char *file_name);
void seek_back_token(struct token_buffer *s, size_t amount);
int get_token(struct token_buffer *s, struct token *out);
int get_token_type(struct token_buffer *s, struct token *out, enum token_type ty);
int peek_token_type(struct token_buffer *s, enum token_type ty);
struct token_metadata *get_token_metadata(const struct token_buffer *toks, size_t position);

#endif
