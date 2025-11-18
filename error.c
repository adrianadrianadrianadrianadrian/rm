#include "error.h"
#include "lexer.h"

void add_error(struct token_buffer *s, struct error *out, char *message)
{
    size_t position = s->current_position;
    if (s->current_position > 1) {
        position -= 1;
    }

    struct token_metadata *tok_metadata = get_token_metadata(s, position);
    struct list_char error_message = list_create(char, 35);
    append_list_char_slice(&error_message, message);
    struct error *boxed = NULL;
    if (out->errored) {
        boxed = malloc(sizeof(*boxed));
        *boxed = *out;
    }

    struct error err = (struct error) {
        .token_metadata = tok_metadata,
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

    fprintf(f, "%s:%d:%d: error: %s\n",
        err->token_metadata->file_name,
        err->token_metadata->row,
        err->token_metadata->col + 1,
        err->error_message.data);

    if (err->inner != NULL) {
        write_error_inner(f, err->inner, ++depth);
    }
}

void write_error(FILE *f, struct error *err)
{
    write_error_inner(f, err, 0);
}
