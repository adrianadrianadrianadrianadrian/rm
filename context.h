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
} scoped_variable;

struct_list(scoped_variable);

struct type_declaration_statement_context {
    struct type type;
    struct list_statement_context *statements;
};

struct while_loop_statement_context {
    struct expression *condition;
    struct statement_context *do_statement;
};

struct if_statement_context {
    struct expression *condition;
    struct statement_context *success_statement;
    struct statement_context *else_statement;
};

typedef struct statement_context {
    enum statement_kind kind;
    union {
        struct expression *expression;
        struct binding_statement *binding_statement;
        struct type_declaration_statement_context type_declaration;
        struct list_statement_context *block_statements;
        struct while_loop_statement_context while_loop_statement;
        struct if_statement_context if_statement_context;
        struct c_block_statement c_block_statement;
    };
    struct global_context *global_context;
    struct list_scoped_variable scoped_variables;
    struct statement_metadata *metadata;
} statement_context;

struct_list(statement_context);

struct rm_program {
    struct global_context global_context;
    struct list_statement_context statements;
};

void contextualise(struct list_statement *s, struct rm_program *out);

#endif
