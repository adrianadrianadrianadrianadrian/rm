#include "ast.h"
#include "context.h"
#include "utils.h"

struct type_check_error {
    struct list_char error_message;
    struct list_char suggestion;
};

int type_eq(struct type *l, struct type *r) {
    return 0;
}

int binding_statement_check(struct binding_statement_context *s, struct type_check_error *error)
{
    if (s->inferred_type == NULL) {
        append_list_char_slice(&error->error_message, "type annotations needed.");
        return 0;
    }
    
    if (s->binding_statement->has_type
        && type_eq(s->inferred_type, &s->binding_statement->variable_type))
    {
        append_list_char_slice(&error->error_message, "mismatch types.");
        return 0;
    }

    return 1;
}

int type_check_single(struct statement_context *s, struct type_check_error *error)
{
    switch (s->kind) {
        case BINDING_STATEMENT:
            return binding_statement_check(&s->binding_statement, error);
        case IF_STATEMENT:
        case RETURN_STATEMENT:
        case BLOCK_STATEMENT:
        case WHILE_LOOP_STATEMENT:
        case TYPE_DECLARATION_STATEMENT:
        {
            for (size_t i = 0; i < s->type_declaration.statements->size; i++) {
                if (!type_check_single(&s->type_declaration.statements->data[i], error)) return 0;
            }
            return 1;
        }
        case BREAK_STATEMENT:
        case SWITCH_STATEMENT:
        {
            return 1;
        }
        case INCLUDE_STATEMENT:
        case ACTION_STATEMENT:
            return 1;
    }

    UNREACHABLE("type_check_single dropped out of a switch on all kinds of statements.");
}

int type_check(struct list_statement_context statements, struct type_check_error *error) {
    for (size_t i = 0; i < statements.size; i++) {
        if (!type_check_single(&statements.data[i], error)) return 0;
    }
    return 1;
}
