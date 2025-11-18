#ifndef ERROR_H
#define ERROR_H

#include "utils.h"
#include "lexer.h"
#include <stdio.h>

struct error {
    struct token_metadata *token_metadata;
    struct list_char error_message;
    int errored;
    struct error *inner;
};

void add_error(struct token_buffer *s, struct error *out, char *message);
void write_error(FILE *f, struct error *error);

#endif
