#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"
#include "error.h"

struct parsed_file {
    struct list_type fn_types;
    struct list_type data_types;
    struct lut_statement_metadata metadata_lookup;
    struct list_statement statements;
};

int parse_file(struct token_buffer *s,
               struct parsed_file *out,
               struct error *error);

#endif
