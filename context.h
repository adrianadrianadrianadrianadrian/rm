#ifndef CONTEXT_H
#define CONTEXT_H

#include "utils.h"
#include "ast.h"

struct global_context {
    struct list_type fn_types;
    struct list_type data_types;
};

typedef struct scoped_variable {
    struct list_char name;
    struct type type;
} scoped_variable;

struct_list(scoped_variable);

struct rm_program {
    struct global_context global_context;
};

void contextualise(struct list_statement *s, struct rm_program *out);

#endif
