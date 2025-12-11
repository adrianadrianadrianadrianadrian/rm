#include "ast.h"
#include "type_inference.h"
#include "utils.h"
#include <assert.h>
#include <string.h>
#include "utils.h"
#include "context.h"
#include "error.h"

#ifdef DEBUG_CONTEXT
void show_statement_context(struct statement_context *s);
#endif

int check_contextual_soundness(struct rm_program *program, struct error *error);

static void add_error_inner(struct statement_metadata *metadata,
                            char *error_message,
                            struct error *out)
{
    add_error(metadata->row, metadata->col, metadata->file_name, out, error_message);
}

int include_statement_global_context(struct statement *s,
                                     struct global_context *gc,
                                     struct error *error)
{
    // TODO
    return 1;
}

int type_declaration_global_context(struct statement *s,
                                    struct global_context *gc,
                                    struct error *error)
{
    assert(s->kind == TYPE_DECLARATION_STATEMENT);
    switch (s->type_declaration.type.kind) {
        case TY_STRUCT:
        case TY_ENUM:
        {
            list_append(&gc->data_types, s->type_declaration.type);
            return 1;
        }
        case TY_FUNCTION:
        {
            list_append(&gc->fn_types, s->type_declaration.type);
            return 1;
        }
        default:
            return 1;
    }
}

int generate_global_context(struct list_statement *s,
                            struct global_context *out,
                            struct error *error)
{
    if (s->size == 0) {
        return 0;
    }

    for (size_t i = 0; i < s->size; i++) {
        struct statement *current = &s->data[i];
        switch (current->kind) {
            case TYPE_DECLARATION_STATEMENT:
                if (!type_declaration_global_context(current, out, error)) return 0;
                break;
            case INCLUDE_STATEMENT:
                if (!include_statement_global_context(current, out, error)) return 0;
                break;
            default:
                break;
        }
    }

    return 1;
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

void add_scoped_variable(struct statement_context *c, struct list_scoped_variable *scoped_variables)
{
    if (c->kind != BINDING_STATEMENT) {
        return;
    }

    struct scoped_variable var = (struct scoped_variable) {
        .name = c->binding_statement.binding_statement->variable_name,
        .defined_type = c->binding_statement.binding_statement->has_type
            ? &c->binding_statement.binding_statement->variable_type
            : NULL,
        .inferred_type = &c->binding_statement.inferred_type
    };
    
    list_append(scoped_variables, var);
}

int contextualise_statement(struct statement *s,
                            struct global_context *global_context,
                            struct list_scoped_variable *scoped_variables,
                            struct error *error,
                            struct statement_context *out)
{
    struct list_char error_message = list_create(char, 100);
    switch (s->kind) {
        case BINDING_STATEMENT:
        {
            struct type inferred_type = {0};
            if (!infer_expression_type(&s->binding_statement.value,
                                       global_context,
                                       scoped_variables,
                                       &inferred_type,
                                       &error_message))
            {
                add_error_inner(&s->metadata, error_message.data, error);
                return 0;
            }

            *out = (struct statement_context) {
                .kind = s->kind,
                .binding_statement = (struct binding_statement_context) {
                    .binding_statement = &s->binding_statement,
                    .inferred_type = inferred_type,
                },
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .metadata = &s->metadata
            };
            return 1;
        }
        case RETURN_STATEMENT:
        {
            struct type inferred_type = {0};
            if (!infer_expression_type(&s->expression,
                                       global_context,
                                       scoped_variables,
                                       &inferred_type,
                                       &error_message))
            {
                add_error_inner(&s->metadata, error_message.data, error);
                return 0;
            }

            *out = (struct statement_context) {
                .kind = s->kind,
                .return_statement = (struct return_statement_context) {
                    .e = (struct expression_context) {
                        .e = &s->expression,
                        .type = inferred_type
                    }
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
                            .defined_type = fn.params.data[i].field_type,
                            .inferred_type = NULL
                        };
                        list_append(&fn_scoped_variables, param);
                    }
                    
                    for (size_t i = 0; i < s->type_declaration.statements->size; i++) {
                        struct statement_context c = {0};
                        if (!contextualise_statement(
                                &s->type_declaration.statements->data[i],
                                global_context,
                                &fn_scoped_variables,
                                error,
                                &c)) 
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
                    break;
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
                    break;
                }
                default:
                    UNREACHABLE("type_declaration type not handled");
            }
            return 1;
        }
        case BLOCK_STATEMENT:
        {
            struct list_scoped_variable block_scoped_variables = copy_scoped_variables(scoped_variables);
            struct list_statement_context *statements = malloc(sizeof(*statements));
            *statements = list_create(statement_context, s->statements->size);

            for (size_t i = 0; i < s->statements->size; i++) {
                struct statement_context c = {0};
                if (!contextualise_statement(
                        &s->statements->data[i],
                        global_context,
                        &block_scoped_variables,
                        error,
                        &c)) 
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
            if (!contextualise_statement(
                    s->if_statement.success_statement,
                    global_context,
                    scoped_variables,
                    error,
                    success))
            {
                return 0;
            }

            if (s->if_statement.else_statement != NULL) {
                if (!contextualise_statement(
                    s->if_statement.else_statement,
                    global_context,
                    scoped_variables,
                    error,
                    else_statement))
                {
                    return 0;
                }
            }

            struct type inferred_condition_type = {0};
            if (!infer_expression_type(&s->if_statement.condition,
                                       global_context,
                                       scoped_variables,
                                       &inferred_condition_type,
                                       &error_message))
            {
                add_error_inner(&s->metadata, error_message.data, error);
                return 0;
            }
            
            *out = (struct statement_context) {
                .kind = s->kind,
                .if_statement_context = (struct if_statement_context) {
                    .condition = (struct expression_context) {
                        .e = &s->if_statement.condition,
                        .type = inferred_condition_type
                    },
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
            struct type inferred_type = {0};
            if (!infer_expression_type(&s->expression,
                                       global_context,
                                       scoped_variables,
                                       &inferred_type,
                                       &error_message))
            {
                add_error_inner(&s->metadata, error_message.data, error);
                return 0;
            }

            *out = (struct statement_context) {
                .kind = s->kind,
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .action_statement_context = (struct action_statement_context) {
                    .e = (struct expression_context) {
                        .e = &s->expression,
                        .type = inferred_type
                    }
                },
                .metadata = &s->metadata
            };
            return 1;
        }
        case WHILE_LOOP_STATEMENT:
        {
            struct statement_context *do_statement = malloc(sizeof(*do_statement));
            if (!contextualise_statement(
                s->while_loop_statement.do_statement,
                global_context,
                scoped_variables,
                error,
                do_statement))
            {
                return 0;
            }
            
            struct type inferred_condition_type = {0};
            if (!infer_expression_type(&s->if_statement.condition,
                                       global_context,
                                       scoped_variables,
                                       &inferred_condition_type,
                                       &error_message))
            {
                add_error_inner(&s->metadata, error_message.data, error);
                return 0;
            }

            *out = (struct statement_context) {
                .kind = s->kind,
                .while_loop_statement = (struct while_loop_statement_context) {
                    .condition = (struct expression_context) {
                        .e = &s->while_loop_statement.condition,
                        .type = inferred_condition_type
                    },
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
        case INCLUDE_STATEMENT:
        {
            *out = (struct statement_context) {
                .kind = s->kind,
                .global_context = global_context,
                .scoped_variables = NULL,
                .include_statement = s->include_statement,
                .metadata = &s->metadata
            };
            return 1;
        }
        case SWITCH_STATEMENT:
            TODO("switch statement, context");
            break;
        default:
            UNREACHABLE("statement kind not handled");
    }

    return 1;
}

int contextualise(struct list_statement *s,
                  struct rm_program *out,
                  struct error *error)
{
    if (s->size == 0) {
        return 0;
    }

    *out = (struct rm_program) {
        .global_context = (struct global_context) {
            .fn_types = list_create(type, 100),
            .data_types = list_create(type, 100)
        },
        .statements = list_create(statement_context, s->size)
    };

    if (!generate_global_context(s, &out->global_context, error)) return 0;
    struct list_scoped_variable scoped_variables = list_create(scoped_variable, 10);

    for (size_t i = 0; i < s->size; i++) {
        struct statement_context c = {0};
        if (!contextualise_statement(&s->data[i], &out->global_context, &scoped_variables, error, &c)) return 0;
        list_append(&out->statements, c);
    }
    
    #ifdef DEBUG_CONTEXT
        for (size_t i = 0; i < out->statements.size; i++) {
            show_statement_context(&out->statements.data[i]);
            printf("\n\n");
        }
    #endif
    
    return check_contextual_soundness(out, error);
}

int check_statement_soundness(struct statement_context *ctx, struct error *error);

int check_expression_soundness(struct expression *e,
                               struct global_context *global_context,
                               struct list_scoped_variable *scoped_variables,
                               struct list_char *error);

int check_literal_expression_soundness(struct literal_expression *e,
                                       struct global_context *global_context,
                                       struct list_scoped_variable *scoped_variables,
                                       struct list_char *error)
{
    switch (e->kind) {
        case LITERAL_NAME:
        {
            for (size_t i = 0; i < scoped_variables->size; i++) {
                struct scoped_variable *var = &scoped_variables->data[i];
                if (list_char_eq(&var->name, e->name)) {
                    return 1;
                }
            }
            return 0;
        }
        case LITERAL_STRUCT:
        case LITERAL_ENUM:
        {
            struct list_char *name = e->struct_enum.name;
            for (size_t i = 0; i < global_context->data_types.size; i++) {
                struct type *data_type = &global_context->data_types.data[i];
                if (list_char_eq(data_type->name, name)) {
                    struct list_key_type_pair *pairs = NULL;
                    if (data_type->kind != TY_ENUM) {
                        pairs = &data_type->enum_type.pairs;
                    } else if (data_type->kind != TY_STRUCT) {
                        pairs = &data_type->struct_type.pairs;
                    } else {
                        return 0;
                    }
                    assert(pairs);
                    
                    for (size_t p = 0; p < pairs->size; p++) {
                        int found = 0;
                        for (size_t l = 0; l < e->struct_enum.key_expr_pairs.size; l++) {
                            struct key_expression *literal_pair = &e->struct_enum.key_expr_pairs.data[l];
                            if (list_char_eq(literal_pair->key, &pairs->data[p].field_name)) {
                                found = 1;
                                if (!check_expression_soundness(literal_pair->expression,
                                                                global_context,
                                                                scoped_variables,
                                                                error))
                                {
                                    return 0;
                                }
                                break;
                            }
                        }

                        if (!found) {
                            append_list_char_slice(error, "required field `");
                            append_list_char_slice(error, pairs->data[p].field_name.data);
                            append_list_char_slice(error, "` is missing.");
                            return 0;
                        }
                        // TODO: check duplicate fields
                    }
                    
                    return 1;
                }
            }
            return 0;
        }
        case LITERAL_HOLE:
        case LITERAL_NULL:
        case LITERAL_BOOLEAN:
        case LITERAL_CHAR:
        case LITERAL_STR:
        case LITERAL_NUMERIC:
            return 1;
    }

    UNREACHABLE("dropped out of check_literal_expression_soundnes switch.");
}

int expression_is_literal_name(struct expression *e)
{
    return e->kind == LITERAL_EXPRESSION && e->literal.kind == LITERAL_NAME;
}

int check_binary_expression_soundness(struct binary_expression *e,
                                      struct global_context *global_context,
                                      struct list_scoped_variable *scoped_variables,
                                      struct list_char *error)
{
    return 1;
}

int check_expression_soundness(struct expression *e,
                               struct global_context *global_context,
                               struct list_scoped_variable *scoped_variables,
                               struct list_char *error)
{
    switch (e->kind) {
        case UNARY_EXPRESSION:
            return check_expression_soundness(e->unary.expression, global_context, scoped_variables, error);
        case LITERAL_EXPRESSION:
            return check_literal_expression_soundness(&e->literal, global_context, scoped_variables, error);
        case GROUP_EXPRESSION:
            return check_expression_soundness(e->grouped, global_context, scoped_variables, error);
        case BINARY_EXPRESSION:
            return check_binary_expression_soundness(&e->binary, global_context, scoped_variables, error);
        case FUNCTION_EXPRESSION:
        case VOID_EXPRESSION:
        case MEMBER_ACCESS_EXPRESSION:
            return 1;
    }

    return 1;
}

typedef struct string {
    struct list_char name;
} string;

struct_list(string);

int check_struct_soundness(struct type *type,
                           struct global_context *global_context,
                           struct list_char *error)
{
    assert(type->kind == TY_STRUCT);
    int struct_count = 0;
    for (size_t i = 0; i < global_context->data_types.size; i++) {
        if (list_char_eq(type->name, global_context->data_types.data[i].name)) {
            struct_count += 1;
            if (struct_count > 1) {
                append_list_char_slice(error, "`struct ");
                append_list_char_slice(error, type->name->data);
                append_list_char_slice(error, "` already exists.");
                return 0;
            }
        }
    }
    
    struct list_string visited = list_create(string, 10);
    struct list_key_type_pair pairs = type->struct_type.pairs;
    for (size_t i = 0; i < pairs.size; i++) {
        struct string field_name = (struct string) {
            .name = pairs.data[i].field_name
        };
        
        for (size_t j = 0; j < visited.size; j++) {
            if (list_char_eq(&visited.data[j].name, &field_name.name)) {
                append_list_char_slice(error, "field `");
                append_list_char_slice(error, field_name.name.data);
                append_list_char_slice(error, "` already exists on struct.");
                return 0;
            }
        }

        list_append(&visited, field_name);
    }
    
    for (size_t i = 0; i < pairs.size; i++) {
        struct type *ty = pairs.data[i].field_type;
        for (size_t m = 0; m < ty->modifiers.size; m++) {
            struct type_modifier *modifier = &ty->modifiers.data[m];
            if (modifier->kind == ARRAY_MODIFIER_KIND
                && modifier->array_modifier.reference_sized)
            {
                struct list_char *ref_name = modifier->array_modifier.reference_name;
                int found = 0;
                for (size_t p = 0; p < pairs.size; p++) {
                    if (list_char_eq(&pairs.data[p].field_name, modifier->array_modifier.reference_name)) {
                        struct type *found_type = pairs.data[p].field_type;
                        if (found_type->kind == TY_PRIMITIVE && found_type->primitive_type == USIZE) {
                            found = 1;
                        } else {
                            append_list_char_slice(error, "`");
                            append_list_char_slice(error, ref_name->data);
                            append_list_char_slice(error, "` must be bound to a field of type `usize`");
                            return 0;
                        }
                    }
                }
                
                if (!found) {
                    append_list_char_slice(error, "`");
                    append_list_char_slice(error, ref_name->data);
                    append_list_char_slice(error, "` is unbounded within `");
                    append_list_char_slice(error, type->name->data);
                    append_list_char_slice(error, "`");
                    return 0;
                }
            }
        }
    }

    return 1;
}

int check_enum_soundness(struct enum_type type,
                         struct global_context *global_context,
                         struct error *error)
{
    TODO("check_enum_soundness");
}

int check_fn_soundness(struct type_declaration_statement_context *type_declaration,
                       struct error *error)
{
    assert(type_declaration->type.kind == TY_FUNCTION);
    for (size_t i = 0; i < type_declaration->statements->size; i++) {
        struct statement_context *this = &type_declaration->statements->data[i];
        if (!check_statement_soundness(this, error)) return 0;
    }
    return 1;
}

int check_binding_statement_soundness(struct statement_context *ctx, struct error *error)
{
    assert(ctx->kind == BINDING_STATEMENT);
    struct list_char error_message = list_create(char, 100);

    for (size_t i = 0; i < ctx->scoped_variables.size; i++) {
        struct list_char *binding_name = &ctx->binding_statement.binding_statement->variable_name;
        if (list_char_eq(&ctx->scoped_variables.data[i].name, binding_name))
        {
            append_list_char_slice(&error_message, "`");
            append_list_char_slice(&error_message, binding_name->data);
            append_list_char_slice(&error_message, "` is already defined in this scope.");
            add_error_inner(ctx->metadata, error_message.data, error);
            return 0;
        }
    }

    if (!check_expression_soundness(&ctx->binding_statement.binding_statement->value,
                                    ctx->global_context,
                                    &ctx->scoped_variables,
                                    &error_message))
    {
        add_error_inner(ctx->metadata, error_message.data, error);
        return 0;
    }

    return 1;
}

int check_if_statement_soundness(struct statement_context *ctx, struct error *error)
{
    assert(ctx->kind == IF_STATEMENT);
    struct if_statement_context *if_ctx = &ctx->if_statement_context;
    struct list_char error_message = list_create(char, 100);

    if (!check_expression_soundness(if_ctx->condition.e,
                                    ctx->global_context,
                                    &ctx->scoped_variables,
                                    &error_message))
    {
        add_error_inner(ctx->metadata, error_message.data, error);
        return 0;
    }
    
    if (!check_statement_soundness(if_ctx->success_statement, error)) return 0;
    if (if_ctx->else_statement
        && !check_statement_soundness(if_ctx->else_statement, error)) return 0;

    return 1;
}

int check_return_statement_soundness(struct statement_context *ctx,
                                     struct error *error)
{
    assert(ctx->kind == RETURN_STATEMENT);
    struct list_char error_message = list_create(char, 100);

    if (!check_expression_soundness(ctx->return_statement.e.e,
                                    ctx->global_context,
                                    &ctx->scoped_variables,
                                    &error_message))
    {
        add_error_inner(ctx->metadata, error_message.data, error);
        return 0;
    }
    
    return 1;
}

int check_block_statement_soundness(struct statement_context *ctx,
                                    struct error *error)
{
    assert(ctx->kind == BLOCK_STATEMENT);
    for (size_t i = 0; i < ctx->block_statements->size; i++) {
        if (!check_statement_soundness(&ctx->block_statements->data[i], error)) return 0;
    }
    return 1;
}

int check_action_statement_soundness(struct statement_context *ctx,
                                     struct error *error)
{
    assert(ctx->kind == ACTION_STATEMENT);
    struct list_char error_message = list_create(char, 100);
    if (!check_expression_soundness(ctx->action_statement_context.e.e,
                                    ctx->global_context,
                                    &ctx->scoped_variables,
                                    &error_message))
    {
        add_error_inner(ctx->metadata, error_message.data, error);
        return 0;
    }
    return 1;
}

int check_while_statement_soundness(struct statement_context *ctx, struct error *error)
{
    assert(ctx->kind == WHILE_LOOP_STATEMENT);
    struct while_loop_statement_context *while_ctx = &ctx->while_loop_statement;
    struct list_char error_message = list_create(char, 100);

    if (!check_expression_soundness(while_ctx->condition.e,
                                    ctx->global_context,
                                    &ctx->scoped_variables,
                                    &error_message))
    {
        add_error_inner(ctx->metadata, error_message.data, error);
        return 0;
    }
    
    if (!check_statement_soundness(while_ctx->do_statement, error)) return 0;

    return 1;
}

int check_statement_soundness(struct statement_context *ctx,
                              struct error *error)
{
    switch (ctx->kind) {
        case RETURN_STATEMENT:
            return check_return_statement_soundness(ctx, error);
        case BINDING_STATEMENT:
            return check_binding_statement_soundness(ctx, error);
        case IF_STATEMENT:
            return check_if_statement_soundness(ctx, error);
        case BLOCK_STATEMENT:
            return check_block_statement_soundness(ctx, error);
        case ACTION_STATEMENT:
            return check_action_statement_soundness(ctx, error);
        case WHILE_LOOP_STATEMENT:
            return check_while_statement_soundness(ctx, error);
        case BREAK_STATEMENT:
        case SWITCH_STATEMENT:
            return 0;
        case TYPE_DECLARATION_STATEMENT:
        case INCLUDE_STATEMENT:
            UNREACHABLE("top level statements shouldn't be here.");
    }

    UNREACHABLE("dropped out of switch in check_statement_soundness.");
}


int check_contextual_soundness(struct rm_program *program, struct error *error)
{
    for (size_t i = 0; i < program->statements.size; i++) {
        struct statement_context ctx = program->statements.data[i];
        switch (ctx.kind) {
            case TYPE_DECLARATION_STATEMENT:
            {
                switch (ctx.type_declaration.type.kind) {
                    case TY_FUNCTION:
                    {
                        if (!check_fn_soundness(&ctx.type_declaration, error)) return 0;
                        break;
                    }
                    case TY_STRUCT:
                    {
                        struct list_char error_message = list_create(char, 100);
                        if (!check_struct_soundness(&ctx.type_declaration.type,
                                                    &program->global_context,
                                                    &error_message))
                        {
                            add_error_inner(ctx.metadata, error_message.data, error);
                            return 0;
                        }
                        break;
                    }
                    case TY_ENUM:
                    {
                        struct list_char error_message = list_create(char, 100);
                        if (!check_enum_soundness(ctx.type_declaration.type.enum_type,
                                                  &program->global_context,
                                                  error))
                        {
                            add_error_inner(ctx.metadata, error_message.data, error);
                            return 0;
                        }
                        break;
                    }
                    case TY_PRIMITIVE:
                        UNREACHABLE("TY_PRIMITIVE shouldn't have made it here via parsing.");
                }
            }
            case INCLUDE_STATEMENT:
                break;
            default:
                UNREACHABLE("statement shouldn't have made it here via parsing.");
        }
    }

    return 1;
}

#ifdef DEBUG_CONTEXT
void print_type(struct type *ty) {
    if (ty == NULL) {
        printf("none");
        return;
    }
    
    if (ty->modifiers.size) {
        printf("mods: %d ", (int)ty->modifiers.size);
    }

    switch (ty->kind) {
        case TY_PRIMITIVE:
        {
            switch (ty->primitive_type) {
                case VOID:
                    printf("void");
                    return;
                case BOOL:
                    printf("bool");
                    return;
                case U8:
                    printf("u8");
                    return;
                case I8:
                    printf("i8");
                    return;
                case I16:
                    printf("i16");
                    return;
                case U16:
                    printf("u16");
                    return;
                case I32:
                    printf("i32");
                    return;
                case U32:
                    printf("u32");
                    return;
                case I64:
                    printf("164");
                    return;
                case U64:
                    printf("u64");
                    return;
                case USIZE:
                    printf("usize");
                    return;
                case F32:
                    printf("f32");
                    return;
                case F64:
                    printf("f64");
                    return;
            }
        }
        case TY_STRUCT:
        {
            printf("struct %s", ty->name->data);
            return;
        }
        case TY_ENUM:
        {
            printf("enum %s", ty->name->data);
            return;
        }
        case TY_FUNCTION:
        {
            printf("fn(");
            for (size_t i = 0; i < ty->function_type.params.size; i++) {
                print_type(ty->function_type.params.data[i].field_type);
                if (i < ty->function_type.params.size - 1) {
                    printf(", ");
                }
            }
            printf(") -> ");
            print_type(ty->function_type.return_type);
            return;
        }
    }
}

void show_global_context(struct global_context *gc) {
    printf("global context\n");
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

    printf("\nscoped variables:\n");
    for (size_t i = 0; i < vars->size; i++) {
        struct scoped_variable scoped = vars->data[i];
        printf("\t%s:", scoped.name.data);
        print_type(scoped.defined_type);
        printf(",");
        print_type(scoped.inferred_type);
        printf("\n");
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
            printf("binding_statement,%s,", s->binding_statement.binding_statement->variable_name.data);
            if (s->binding_statement.binding_statement->has_type) {
                print_type(&s->binding_statement.binding_statement->variable_type);
            } else {
                printf("none");
            }
            printf(",");
            print_type(&s->binding_statement.inferred_type);
            show_scoped_variables(&s->scoped_variables);
            break;
        }
        case TYPE_DECLARATION_STATEMENT:
        {
            switch (s->type_declaration.type.kind) {
                case TY_FUNCTION:
                {
                    printf("function,%s,", s->type_declaration.type.name->data);
                    print_type(&s->type_declaration.type);
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
            printf("return_statement,");
            print_type(&s->return_statement.e.type);
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
        case INCLUDE_STATEMENT:
        {
            printf("include,%s", s->include_statement.include.data);
            break;
        }
        default:
            break;
    }
}
#endif
