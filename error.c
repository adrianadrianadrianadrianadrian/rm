#include "error.h"

#define NO_COLOUR "\x1B[0m"
#define RED "\x1B[31m"

void add_error(unsigned int row,
               unsigned int col,
               char *file_name,
               struct error *out,
               char *message)
{
    struct list_char error_message = list_create(char, 35);
    append_list_char_slice(&error_message, message);
    struct error *boxed = NULL;
    if (out->errored) {
        boxed = malloc(sizeof(*boxed));
        *boxed = *out;
    }

    struct error err = (struct error) {
        .row = row,
        .col = row,
        .file_name = file_name,
        .errored = 1,
        .error_message = error_message,
        .inner = boxed
    };
        
    *out = err;
}

void write_error_inner(FILE *f, struct error *err, unsigned int depth)
{
    if (!err->errored) {
        return;
    }

    fprintf(f, "%s:%d:%d: %serror:%s %s\n",
        err->file_name,
        err->row,
        err->col + 1,
        RED,
        NO_COLOUR,
        err->error_message.data);

    if (err->inner != NULL) {
        write_error_inner(f, err->inner, ++depth);
    }
}

void write_error(FILE *f, struct error *err)
{
    write_error_inner(f, err, 0);
}
