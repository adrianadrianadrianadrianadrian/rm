#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

int parse_rm_file(struct token_buffer *s, struct rm_file *out);

#endif
