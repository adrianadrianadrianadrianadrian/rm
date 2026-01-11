#include "ast.h"
#include "parser.h"
#include "utils.h"
#include <assert.h>
#include <string.h>
#include "utils.h"
#include "context.h"
#include "type_inference.h"

static void add_error_inner(struct statement_metadata *metadata,
                            char *error_message,
                            struct error *out)
{
    add_error(metadata->row, metadata->col, metadata->file_name, out, error_message);
}

struct list_scoped_variable copy_scoped_variables(struct list_scoped_variable *scoped)
{
    if (scoped->size == 0) {
        return list_create(scoped_variable, scoped->capacity);
    }

    struct scoped_variable *data = malloc(sizeof(*data) * scoped->capacity);
    memcpy(data, scoped->data, scoped->size * sizeof(*data));
    return (struct list_scoped_variable) {
        .data = data,
        .size = scoped->size,
        .capacity = scoped->capacity
    };
}

void add_scoped_variable(struct statement *s,
                         struct lut_type *expression_types,
                         struct list_scoped_variable *scoped_variables)
{
    if (s->kind != BINDING_STATEMENT) {
        return;
    }

    struct scoped_variable var = (struct scoped_variable) {
        .name = s->binding_statement.variable_name,
        .type = lut_get(expression_types, s->binding_statement.value.id)
    };

    list_append(scoped_variables, var);
}

int contextualise_statement(struct statement *s,
                            struct global_context *global_context,
                            struct list_scoped_variable *scoped_variables,
                            struct context *context,
                            struct error *error)
{
    struct list_char error_message = list_create(char, 100);
    if (scoped_variables == NULL) {
        *scoped_variables = list_create(scoped_variable, 10);
    }

    switch (s->kind) {
        case BINDING_STATEMENT:
        {
            struct type value_type = {0};
            if (!infer_expression_type(&s->binding_statement.value,
                                       global_context,
                                       scoped_variables,
                                       &value_type,
                                       &error_message))
            {
                struct statement_metadata metadata = lut_get(&global_context->metadata_lookup, s->id);
                add_error_inner(&metadata, error_message.data, error);
                return 0;
            }

            struct statement_scope scope = {
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            lut_add(&context->statement_scope_lookup, s->id, scope);
            lut_add(&context->expression_type_lookup, s->binding_statement.value.id, value_type);
            return 1;
        }
        case RETURN_STATEMENT:
        {
            struct type return_type = {0};
            if (!infer_expression_type(&s->expression,
                                       global_context,
                                       scoped_variables,
                                       &return_type,
                                       &error_message))
            {
                struct statement_metadata metadata = lut_get(&global_context->metadata_lookup, s->id);
                add_error_inner(&metadata, error_message.data, error);
                return 0;
            }

            struct statement_scope scope = {
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            lut_add(&context->statement_scope_lookup, s->id, scope);
            lut_add(&context->expression_type_lookup, s->expression.id, return_type);
            return 1;
        }
        case TYPE_DECLARATION_STATEMENT:
        {
            switch (s->type_declaration.type.kind) {
                case TY_FUNCTION:
                {
                    struct list_scoped_variable fn_scoped_variables = copy_scoped_variables(scoped_variables);
                    struct function_type fn = s->type_declaration.type.function_type;
                    for (size_t i = 0; i < fn.params.size; i++) {
                        struct scoped_variable param = (struct scoped_variable) {
                            .name = fn.params.data[i].field_name,
                            .type = infer_full_type(fn.params.data[i].field_type,
                                                    global_context,
                                                    scoped_variables)
                        };
                        list_append(&fn_scoped_variables, param);
                    }
                    
                    for (size_t i = 0; i < s->type_declaration.statements->size; i++) {
                        struct statement *this = &s->type_declaration.statements->data[i];
                        if (!contextualise_statement(this,
                                                     global_context,
                                                     &fn_scoped_variables,
                                                     context,
                                                     error))
                        {
                            return 0;
                        }
                        add_scoped_variable(this, &context->expression_type_lookup, &fn_scoped_variables);
                    }

                    struct statement_scope scope = {
                        .scoped_variables = copy_scoped_variables(scoped_variables)
                    };
                    lut_add(&context->statement_scope_lookup, s->id, scope);
                    return 1;
                }
                case TY_STRUCT:
                case TY_ENUM:
                {
                    struct statement_scope scope = {
                        .scoped_variables = list_create(scoped_variable, 1)
                    };
                    lut_add(&context->statement_scope_lookup, s->id, scope);
                    return 1;
                }
                case TY_PRIMITIVE:
                    UNREACHABLE("type_declaration type cannot be TY_PRIMITIVE");
                default:
                    UNREACHABLE("type_declaration type not handled");
            }
        }
        case BLOCK_STATEMENT:
        {
            struct list_scoped_variable block_scoped_variables = copy_scoped_variables(scoped_variables);
            for (size_t i = 0; i < s->statements->size; i++) {
                struct statement *this = &s->type_declaration.statements->data[i];
                if (!contextualise_statement(this,
                                             global_context,
                                             &block_scoped_variables,
                                             context,
                                             error))
                {
                    return 0;
                }
                add_scoped_variable(this, &context->expression_type_lookup, &block_scoped_variables);
            }

            struct statement_scope scope = {
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            lut_add(&context->statement_scope_lookup, s->id, scope);
            return 1;
        }
        case IF_STATEMENT:
        {
            if (!contextualise_statement(s->if_statement.success_statement,
                                         global_context,
                                         scoped_variables,
                                         context,
                                         error))
            {
                return 0;
            }

            struct statement_scope success_scope = {
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            lut_add(&context->statement_scope_lookup, s->if_statement.success_statement->id, success_scope);

            if (s->if_statement.else_statement != NULL) {
                if (!contextualise_statement(s->if_statement.else_statement,
                                             global_context,
                                             scoped_variables,
                                             context,
                                             error))
                {
                    return 0;
                }
                struct statement_scope scope = {
                    .scoped_variables = copy_scoped_variables(scoped_variables)
                };
                lut_add(&context->statement_scope_lookup, s->if_statement.else_statement->id, scope);
            }
            
            struct type condition_type = {0};
            if (!infer_expression_type(&s->if_statement.condition,
                                       global_context,
                                       scoped_variables,
                                       &condition_type,
                                       &error_message))
            {
                struct statement_metadata metadata = lut_get(&global_context->metadata_lookup, s->id);
                add_error_inner(&metadata, error_message.data, error);
                return 0;
            }

            struct statement_scope scope = {
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            lut_add(&context->statement_scope_lookup, s->id, scope);
            lut_add(&context->expression_type_lookup, s->if_statement.condition.id, condition_type);
            return 1;
        }
        case ACTION_STATEMENT:
        {
            struct type action_type = {0};
            if (!infer_expression_type(&s->expression,
                                       global_context,
                                       scoped_variables,
                                       &action_type,
                                       &error_message))
            {
                struct statement_metadata metadata = lut_get(&global_context->metadata_lookup, s->id);
                add_error_inner(&metadata, error_message.data, error);
                return 0;
            }
            struct statement_scope scope = {
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            lut_add(&context->statement_scope_lookup, s->id, scope);
            lut_add(&context->expression_type_lookup, s->expression.id, action_type);
            return 1;
        }
        case WHILE_LOOP_STATEMENT:
        {
            if (!contextualise_statement(s->while_loop_statement.do_statement,
                                         global_context,
                                         scoped_variables,
                                         context,
                                         error))
            {
                return 0;
            }
            struct statement_scope do_statement_scope = {
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            lut_add(&context->statement_scope_lookup, s->while_loop_statement.do_statement->id, do_statement_scope);

            struct type condition_type = {0};
            if (!infer_expression_type(&s->while_loop_statement.condition,
                                       global_context,
                                       scoped_variables,
                                       &condition_type,
                                       &error_message))
            {
                struct statement_metadata metadata = lut_get(&global_context->metadata_lookup, s->id);
                add_error_inner(&metadata, error_message.data, error);
                return 0;
            }
            struct statement_scope while_scope = {
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            lut_add(&context->statement_scope_lookup, s->id, while_scope);
            lut_add(&context->expression_type_lookup, s->while_loop_statement.condition.id, condition_type);
            return 1;
        }
        case BREAK_STATEMENT:
        {
            struct statement_scope scope = {
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            lut_add(&context->statement_scope_lookup, s->id, scope);
            return 1;
        }
        case SWITCH_STATEMENT:
            TODO("switch statement, context");
            return 0;
        case C_BLOCK_STATEMENT:
        {
            struct statement_scope scope = {
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            lut_add(&context->statement_scope_lookup, s->id, scope);
            return 1;
        }
        default:
            UNREACHABLE("statement kind not handled");
    }
}

int contextualise(struct parsed_file *parsed_file, struct context *out, struct error *error)
{
    struct context output = {
        .expression_type_lookup = lut_create(type, 100),
        .statement_scope_lookup = lut_create(statement_scope, 100)
    };
    assert(parsed_file->statements.size > 0);

    for (size_t i = 0; i < parsed_file->statements.size; i++) {
        if (!contextualise_statement(&parsed_file->statements.data[i],
                                     &parsed_file->global_context,
                                     NULL,
                                     &output,
                                     error))
        {
            return 0;
        }
    }
    
    *out = output;
    return 1;
}
