#ifndef ERROR_H
#define ERROR_H

#include "utils.h"
#include "lexer.h"
#include <stdio.h>

struct error {
    unsigned int row;
    unsigned int col;
    unsigned int errored;
    char *file_name;
    struct list_char error_message;
    struct error *inner;
};

void add_error(unsigned int row,
               unsigned int col,
               char *file_name,
               struct error *out,
               char *message);

void write_error(FILE *f, struct error *error);
void write_raw_error(FILE *f, char *error);

#endif
