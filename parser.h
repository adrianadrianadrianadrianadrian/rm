#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"
#include "error.h"

struct parse_error {
    struct token_metadata *token_metadata;
    struct list_char error_message;
    int errored;
    struct parse_error *parent;
};

int parse_rm_file(struct token_buffer *s, struct list_statement *out, struct parse_error *error);

#endif
