#ifndef CONTEXT_H
#define CONTEXT_H

#include "utils.h"
#include "ast.h"

typedef struct scoped_variable {
    struct list_char name;
    struct type type;
} scoped_variable;

struct_list(scoped_variable);
struct_list(statement_metadata);

struct context {
    struct list_type fn_types;
    struct list_type data_types;
    struct list_statement_metadata metadata_by_statement_id;
    struct list_scoped_variable scoped_variables_by_statement_id;
    struct list_type type_by_expression_id;
};

void add_scoped_variable(struct context *c,
                         struct scoped_variable *var,
                         unsigned long statement_id);

struct list_scoped_variable
*get_statement_scoped_variables(struct context *c,
                                unsigned long statement_id);

void add_expression_type(struct context *c,
                         struct type type,
                         unsigned long expression_id);

struct type *get_expression_type(struct context *c,
                                 unsigned long expression_id);

#endif
