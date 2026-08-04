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
    FN_KEYWORD            = 5863385L,       // fn
    ENUM_KEYWORD          = 6385194298L,    // enum
    STRUCT_KEYWORD        = 6954031505834L, // struct
    IF_KEYWORD            = 5863476L,       // if
    WHILE_KEYWORD         = 210732529790L,  // while
    RETURN_KEYWORD        = 6953974653989L, // return
    BOOLEAN_TRUE_KEYWORD  = 6385737701L,    // true
    BOOLEAN_FALSE_KEYWORD = 210712121072L,  // false
    ELSE_KEYWORD          = 6385192046L,    // else
    BREAK_KEYWORD         = 210707980106L,  // break
    MUTABLE_KEYWORD       = 193499675L,     // mut
    NULL_KEYWORD          = 6385525056L,    // null
    SWITCH_KEYWORD        = 6954034739063L, // switch
    CASE_KEYWORD          = 6385108193L,    // case
    LET_KEYWORD           = 193498058L,     // let

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
