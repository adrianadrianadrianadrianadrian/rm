#ifndef CONTEXT_H
#define CONTEXT_H

#include "utils.h"
#include "ast.h"
#include "parser.h"
#include "error.h"

typedef struct scoped_variable {
    struct list_char name;
    struct type type;
} scoped_variable;

struct_list(scoped_variable);

typedef struct statement_scope {
    struct list_scoped_variable scoped_variables;
} statement_scope;

struct_lut(statement_scope);

struct context {
    struct lut_statement_scope statement_scope_lookup;
    struct lut_type expression_type_lookup;
};

int contextualise(struct parsed_file *parsed_file,
                  struct context *out,
                  struct error *error);

#endif
