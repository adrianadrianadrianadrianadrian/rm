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
    struct type *defined_type;
    struct type *inferred_type;
} scoped_variable;

struct_list(scoped_variable);

struct binding_statement_context {
    struct type *inferred_type;
    struct binding_statement *binding_statement;
};

struct return_statement_context {
    struct expression *e;
    struct type *inferred_return_type;
};

struct type_declaration_statement_context {
    struct type type;
    struct list_statement_context *statements;
};

struct while_loop_statement_context {
    struct expression condition;
    struct statement_context *do_statement;
};

struct if_statement_context {
    struct expression condition;
    struct statement_context *success_statement;
    struct statement_context *else_statement;
};

typedef struct statement_context {
    enum statement_kind kind;
    union {
        struct binding_statement_context binding_statement;
        struct return_statement_context return_statement;
        struct type_declaration_statement_context type_declaration;
        struct list_statement_context *block_statements;
        struct while_loop_statement_context while_loop_statement;
        struct if_statement_context if_statement_context;
        struct include_statement include_statement;
    };
    struct global_context *global_context;
    struct list_scoped_variable scoped_variables;
} statement_context;

struct_list(statement_context);

struct context_error {
    struct list_char message;
};

int contextualise(struct list_statement *s,
                  struct list_statement_context *out,
                  struct context_error *error);

#endif
