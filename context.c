#include "ast.h"
#include "utils.h"
#include <assert.h>
#include <string.h>
#include "utils.h"
#include "context.h"
#include "type_inference.h"

#ifdef DEBUG_CONTEXT
void show_statement_context(struct statement_context *s);
void show_global_context(struct global_context *gc);
#endif

static void add_error_inner(struct statement_metadata *metadata,
                            char *error_message,
                            struct error *out)
{
    add_error(metadata->row, metadata->col, metadata->file_name, out, error_message);
}

void type_declaration_global_context(struct statement *s,
                                     struct global_context *gc)
{
    assert(s->kind == TYPE_DECLARATION_STATEMENT);
    switch (s->type_declaration.type.kind) {
        case TY_STRUCT:
        case TY_ENUM:
        {
            list_append(&gc->data_types, s->type_declaration.type);
            return;
        }
        case TY_FUNCTION:
        {
            list_append(&gc->fn_types, s->type_declaration.type);
            return;
        }
        default:
            return;
    }
}

void generate_global_context(struct list_statement *s,
                             struct global_context *out)
{
    if (s->size == 0) {
        return;
    }

    for (size_t i = 0; i < s->size; i++) {
        struct statement *current = &s->data[i];
        switch (current->kind) {
            case TYPE_DECLARATION_STATEMENT:
                type_declaration_global_context(current, out);
                break;
            default:
                break;
        }
    }
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

void add_scoped_variable(struct statement_context *c,
                         struct list_scoped_variable *scoped_variables)
{
    if (c->kind != BINDING_STATEMENT) {
        return;
    }

    struct scoped_variable var = (struct scoped_variable) {
        .name = c->binding_statement.binding_statement->variable_name,
        .type = c->binding_statement.value_type
    };
    
    list_append(scoped_variables, var);
}

int contextualise_statement(struct statement *s,
                            struct global_context *global_context,
                            struct list_scoped_variable *scoped_variables,
                            struct statement_context *out,
                            struct error *error)
{
    struct list_char error_message = list_create(char, 100);
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
                add_error_inner(&s->metadata, error_message.data, error);
                return 0;
            }
                                       
            *out = (struct statement_context) {
                .kind = s->kind,
                .binding_statement = (struct binding_statement_context) {
                    .binding_statement = &s->binding_statement,
                    .value_type = value_type
                },
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .metadata = &s->metadata
            };

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
                add_error_inner(&s->metadata, error_message.data, error);
                return 0;
            }

            *out = (struct statement_context) {
                .kind = s->kind,
                .expression = (struct expression_context) {
                    .expression = &s->expression,
                    .type = return_type
                },
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .metadata = &s->metadata
            };

            return 1;
        }
        case TYPE_DECLARATION_STATEMENT:
        {
            switch (s->type_declaration.type.kind) {
                case TY_FUNCTION:
                {
                    struct list_scoped_variable fn_scoped_variables = copy_scoped_variables(scoped_variables);
                    struct function_type fn = s->type_declaration.type.function_type;
                    struct list_statement_context *statements = malloc(sizeof(*statements));
                    *statements = list_create(statement_context, s->type_declaration.statements->size);
                    
                    for (size_t i = 0; i < fn.params.size; i++) {
                        struct scoped_variable param = (struct scoped_variable) {
                            .name = fn.params.data[i].field_name,
                            // TODO: infer type
                        };
                        list_append(&fn_scoped_variables, param);
                    }
                    
                    for (size_t i = 0; i < s->type_declaration.statements->size; i++) {
                        struct statement_context c = {0};
                        if (!contextualise_statement(&s->type_declaration.statements->data[i],
                                                     global_context,
                                                     &fn_scoped_variables,
                                                     &c,
                                                     error))
                        {
                            return 0;
                        }
                        list_append(statements, c);
                        add_scoped_variable(&c, &fn_scoped_variables);
                    }
                    
                    *out = (struct statement_context) {
                        .kind = s->kind,
                        .type_declaration = (struct type_declaration_statement_context) {
                            .type = s->type_declaration.type,
                            .statements = statements
                        },
                        .global_context = global_context,
                        .scoped_variables = copy_scoped_variables(&fn_scoped_variables),
                        .metadata = &s->metadata
                    };

                    return 1;
                }
                case TY_PRIMITIVE:
                case TY_STRUCT:
                case TY_ENUM:
                {
                    *out = (struct statement_context) {
                        .kind = s->kind,
                        .type_declaration = (struct type_declaration_statement_context) {
                            .type = s->type_declaration.type,
                            .statements = NULL
                        },
                        .global_context = global_context,
                        .scoped_variables = NULL,
                        .metadata = &s->metadata
                    };
                    return 1;
                }
                default:
                    UNREACHABLE("type_declaration type not handled");
            }
        }
        case BLOCK_STATEMENT:
        {
            struct list_scoped_variable block_scoped_variables = copy_scoped_variables(scoped_variables);
            struct list_statement_context *statements = malloc(sizeof(*statements));
            *statements = list_create(statement_context, s->statements->size);

            for (size_t i = 0; i < s->statements->size; i++) {
                struct statement_context c = {0};
                if (!contextualise_statement(&s->statements->data[i],
                                             global_context,
                                             &block_scoped_variables,
                                             &c,
                                             error))
                {
                    return 0;
                }
                list_append(statements, c);
                add_scoped_variable(&c, &block_scoped_variables);
            }

            *out = (struct statement_context) {
                .kind = s->kind,
                .block_statements = statements,
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .metadata = &s->metadata
            };

            return 1;
        }
        case IF_STATEMENT:
        {
            struct statement_context *success = malloc(sizeof(*success));
            struct statement_context *else_statement = malloc(sizeof(*else_statement));
            if (!contextualise_statement(s->if_statement.success_statement,
                                         global_context,
                                         scoped_variables,
                                         success,
                                         error))
            {
                return 0;
            }

            if (s->if_statement.else_statement != NULL) {
                if (!contextualise_statement(s->if_statement.else_statement,
                                        global_context,
                                        scoped_variables,
                                        else_statement,
                                        error))
                {
                    return 0;
                }
            }
            
            struct type condition_type = {0};
            if (!infer_expression_type(&s->if_statement.condition,
                                       global_context,
                                       scoped_variables,
                                       &condition_type,
                                       &error_message))
            {
                add_error_inner(&s->metadata, error_message.data, error);
                return 0;
            }
            
            *out = (struct statement_context) {
                .kind = s->kind,
                .if_statement_context = (struct if_statement_context) {
                    .condition = &s->if_statement.condition,
                    .condition_type = condition_type,
                    .success_statement = success,
                    .else_statement = s->if_statement.else_statement != NULL
                        ? else_statement
                        : NULL
                },
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .metadata = &s->metadata
            };

            return 1;
        }
        case ACTION_STATEMENT:
        {
            struct type action_type = {0};
            if (!infer_expression_type(&s->if_statement.condition,
                                       global_context,
                                       scoped_variables,
                                       &action_type,
                                       &error_message))
            {
                add_error_inner(&s->metadata, error_message.data, error);
                return 0;
            }

            *out = (struct statement_context) {
                .kind = s->kind,
                .expression = (struct expression_context) {
                    .expression = &s->expression,
                    .type = action_type
                },
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .metadata = &s->metadata
            };

            return 1;
        }
        case WHILE_LOOP_STATEMENT:
        {
            struct statement_context *do_statement = malloc(sizeof(*do_statement));
            if (!contextualise_statement(s->while_loop_statement.do_statement,
                                         global_context,
                                         scoped_variables,
                                         do_statement,
                                         error))
            {
                return 0;
            }

            struct type condition_type = {0};
            if (!infer_expression_type(&s->while_loop_statement.condition,
                                       global_context,
                                       scoped_variables,
                                       &condition_type,
                                       &error_message))
            {
                add_error_inner(&s->metadata, error_message.data, error);
                return 0;
            }

            *out = (struct statement_context) {
                .kind = s->kind,
                .while_loop_statement = (struct while_loop_statement_context) {
                    .condition = &s->while_loop_statement.condition,
                    .condition_type = condition_type,
                    .do_statement = do_statement
                },
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .global_context = global_context,
                .metadata = &s->metadata
            };

            return 1;
        }
        case BREAK_STATEMENT:
        {
            *out = (struct statement_context) {
                .kind = s->kind,
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .global_context = global_context,
                .metadata = &s->metadata
            };

            return 1;
        }
        case SWITCH_STATEMENT:
            TODO("switch statement, context");
            return 0;
        case C_BLOCK_STATEMENT:
        {
            *out = (struct statement_context) {
                .kind = s->kind,
                .c_block_statement = s->c_block_statement,
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .metadata = &s->metadata
            };
            return 1;
        }
        default:
            UNREACHABLE("statement kind not handled");
    }
}

void contextualise(struct list_statement *s, struct rm_program *out)
{
    if (s->size == 0) {
        return;
    }

    *out = (struct rm_program) {
            .global_context = (struct global_context) {
                .fn_types = list_create(type, 100),
                .data_types = list_create(type, 100)
            },
            .statements = list_create(statement_context, s->size)
        };

    generate_global_context(s, &out->global_context);
    struct list_scoped_variable scoped_variables = list_create(scoped_variable, 10);

    for (size_t i = 0; i < s->size; i++) {
        struct statement_context c = {0};
        contextualise_statement(&s->data[i],
                                &out->global_context,
                                &scoped_variables,
                                &c);
        list_append(&out->statements, c);

#ifdef DEBUG_CONTEXT
        show_statement_context(&c);
        printf("\n");
#endif
    }
}

#ifdef DEBUG_CONTEXT
void show_global_context(struct global_context *gc) {
    printf("global context:\n");
    printf("functions [");
    for (size_t i = 0; i < gc->fn_types.size; i++) {
        printf("%s", gc->fn_types.data[i].name->data);
        if (i < gc->fn_types.size - 1) {
           printf(", "); 
        }
    }
    printf("]\n");
    printf("datatypes [");
    for (size_t i = 0; i < gc->data_types.size; i++) {
        printf("%s", gc->data_types.data[i].name->data);
        if (i < gc->data_types.size - 1) {
           printf(", "); 
        }
    }
    printf("]\n");
}

void show_scoped_variables(struct list_scoped_variable *vars)
{
    if (vars->size <= 0) {
        return;
    }

    printf(" | scoped variables: ");
    for (size_t i = 0; i < vars->size; i++) {
        struct scoped_variable scoped = vars->data[i];
        printf("%s,", scoped.name.data);
    }
}

void show_statement_context(struct statement_context *s)
{
    static int global_shown;
    if (!global_shown) {
        show_global_context(s->global_context);
        global_shown = 1;
    }

    switch (s->kind) {
        case BINDING_STATEMENT:
        {
            printf("binding_statement:%s", s->binding_statement.binding_statement->variable_name.data);
            show_scoped_variables(&s->scoped_variables);
            break;
        }
        case TYPE_DECLARATION_STATEMENT:
        {
            switch (s->type_declaration.type.kind) {
                case TY_FUNCTION:
                {
                    printf("function,%s,", s->type_declaration.type.name->data);
                    for (size_t i = 0; i < s->type_declaration.statements->size; i++) {
                        printf("\n");
                        show_statement_context(&s->type_declaration.statements->data[i]);
                    }
                }
                default:
                    break;
            }

            break;
        }
        case RETURN_STATEMENT:
        {
            printf("return_statement");
            show_scoped_variables(&s->scoped_variables);
            break;
        }
        case WHILE_LOOP_STATEMENT:
        {
            printf("while_statement");
            show_scoped_variables(&s->scoped_variables);
            printf("\n");
            show_statement_context(s->while_loop_statement.do_statement);
            break;
        }
        case BLOCK_STATEMENT:
        {
            printf("block_statement");
            for (size_t i = 0; i < s->block_statements->size; i++) {
                printf("\n");
                show_statement_context(&s->block_statements->data[i]);
            }
            break;
        }
        case BREAK_STATEMENT:
        {
            printf("break_statement");
            show_scoped_variables(&s->scoped_variables);
            break;
        }
        case ACTION_STATEMENT:
        {
            printf("action_statement");
            show_scoped_variables(&s->scoped_variables);
            break;
        }
        case IF_STATEMENT:
        {
            printf("if_statement");
            show_scoped_variables(&s->scoped_variables);
            printf("\n");
            printf("success_condition");
            printf("\n");
            show_statement_context(s->if_statement_context.success_statement);
            if (s->if_statement_context.else_statement != NULL) {
                printf("\n");
                printf("else_condition");
                printf("\n");
                show_statement_context(s->if_statement_context.else_statement);
            }
            break;
        }
        default:
            break;
    }
}
#endif
