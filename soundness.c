#include "soundness.h"
#include "ast.h"
#include "context.h"
#include "parser.h"
#include "error.h"
#include "utils.h"
#include <assert.h>

static void add_error_inner(struct statement_metadata *metadata,
                            char *error_message,
                            struct error *out)
{
    add_error(metadata->row, metadata->col, metadata->file_name, out, error_message);
}

int check_statement_soundness(struct statement *s,
                              struct global_context *global_context,
                              struct context *context,
                              struct error *error);

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
            
            for (size_t i = 0; i < global_context->fn_types.size; i++) {
                struct type *fn_type = &global_context->fn_types.data[i];
                if (list_char_eq(e->name, fn_type->name)) {
                    return 1;
                }
            }

            append_list_char_slice(error, "`");
            append_list_char_slice(error, e->name->data);
            append_list_char_slice(error, "` is not in the current scope.");
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
                        UNREACHABLE("data types are either enums or structs.");
                    }
                    assert(pairs);
                    
                    if (pairs->size < e->struct_enum.key_expr_pairs.size) {
                        append_list_char_slice(error, "too many fields provided.");
                        return 0;
                    }
                    
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

int check_expression_soundness(struct expression *e,
                               struct global_context *global_context,
                               struct list_scoped_variable *scoped_variables,
                               struct list_char *error)
{
    switch (e->kind) {
        case UNARY_EXPRESSION:
            return check_expression_soundness(e->unary.expression,
                                              global_context,
                                              scoped_variables,
                                              error);
        case LITERAL_EXPRESSION:
            return check_literal_expression_soundness(&e->literal,
                                                      global_context,
                                                      scoped_variables,
                                                      error);
        case GROUP_EXPRESSION:
            return check_expression_soundness(e->grouped,
                                              global_context,
                                              scoped_variables,
                                              error);
        case BINARY_EXPRESSION:
            return check_expression_soundness(e->binary.l, global_context, scoped_variables, error)
                && check_expression_soundness(e->binary.r, global_context, scoped_variables, error);
        case FUNCTION_EXPRESSION:
        {
            return 1;
        }
        case VOID_EXPRESSION:
        case MEMBER_ACCESS_EXPRESSION:
            return 1;
    }

    return 1;
}

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
            if (modifier->kind == ARRAY_MODIFIER_KIND)
            {
                if (modifier->array_modifier.reference_sized) {
                    // It's sized, so let's check the size is bound within the struct.
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
                } else {
                    // It's not sized, let's ensure it's a pointer.
                    if (m >= 1 && ty->modifiers.data[m - 1].kind == POINTER_MODIFIER_KIND) {
                        continue;
                    }

                    append_list_char_slice(error, "`");
                    append_list_char_slice(error, pairs.data[i].field_name.data);
                    append_list_char_slice(error, "` must have a pointer modifier");
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

int check_fn_soundness(struct type_declaration_statement *type_declaration,
                       struct global_context *global_context,
                       struct context *context,
                       struct error *error)
{
    assert(type_declaration->type.kind == TY_FUNCTION);
    for (size_t i = 0; i < type_declaration->statements->size; i++) {
        struct statement *this = &type_declaration->statements->data[i];
        if (!check_statement_soundness(this, global_context, context, error)) return 0;
    }
    return 1;
}

int check_binding_statement_soundness(struct statement *s,
                                      struct global_context *global_context,
                                      struct context *context,
                                      struct error *error)
{
    assert(s->kind == BINDING_STATEMENT);
    struct list_char error_message = list_create(char, 100);
    struct list_scoped_variable *scoped_variables =
        &lut_get(&context->statement_scope_lookup, s->id).scoped_variables;
    struct list_char *binding_name = &s->binding_statement.variable_name;

    for (size_t i = 0; i < scoped_variables->size; i++) {
        if (list_char_eq(binding_name, &scoped_variables->data[i].name)) {
            append_list_char_slice(&error_message, "the binding name `");
            append_list_char_slice(&error_message, binding_name->data);
            append_list_char_slice(&error_message, "` is already defined in this scope.");
            struct statement_metadata metadata =
                lut_get(&global_context->metadata_lookup, s->id);
            add_error_inner(&metadata, error_message.data, error);
            return 0;
        }
    }
    
    for (size_t i = 0; i < global_context->fn_types.size; i++) {
        if (list_char_eq(binding_name, global_context->fn_types.data[i].name)) {
            append_list_char_slice(&error_message, "the binding name `");
            append_list_char_slice(&error_message, binding_name->data);
            append_list_char_slice(&error_message, "` conflicts with a function in this scope.");
            struct statement_metadata metadata =
                lut_get(&global_context->metadata_lookup, s->id);
            add_error_inner(&metadata, error_message.data, error);
            return 0;
        }
    }

    if (!check_expression_soundness(&s->binding_statement.value,
                                    global_context,
                                    scoped_variables,
                                    &error_message))
    {
        struct statement_metadata metadata =
            lut_get(&global_context->metadata_lookup, s->id);
        add_error_inner(&metadata, error_message.data, error);
        return 0;
    }

    return 1;
}

int check_if_statement_soundness(struct statement *s,
                                 struct global_context *global_context,
                                 struct context *context,
                                 struct error *error)
{
    assert(s->kind == IF_STATEMENT);
    struct if_statement *if_statement= &s->if_statement;
    struct list_char error_message = list_create(char, 100);
    struct list_scoped_variable *scoped_variables =
        &lut_get(&context->statement_scope_lookup, s->id).scoped_variables;

    if (!check_expression_soundness(&if_statement->condition,
                                    global_context,
                                    scoped_variables,
                                    &error_message))
    {
        struct statement_metadata metadata =
            lut_get(&global_context->metadata_lookup, s->id);
        add_error_inner(&metadata, error_message.data, error);
        return 0;
    }
    
    if (!check_statement_soundness(if_statement->success_statement,
                                   global_context,
                                   context,
                                   error))
    {
        return 0;
    }

    if (if_statement->else_statement
        && !check_statement_soundness(if_statement->else_statement,
                                      global_context,
                                      context,
                                      error))
    {
        return 0;
    }

    return 1;
}

int check_return_statement_soundness(struct statement *s,
                                     struct global_context *global_context,
                                     struct context *context,
                                     struct error *error)
{
    assert(s->kind == RETURN_STATEMENT);
    struct list_char error_message = list_create(char, 100);
    struct list_scoped_variable *scoped_variables =
        &lut_get(&context->statement_scope_lookup, s->id).scoped_variables;

    if (!check_expression_soundness(&s->expression,
                                    global_context,
                                    scoped_variables,
                                    &error_message))
    {
        struct statement_metadata metadata =
            lut_get(&global_context->metadata_lookup, s->id);
        add_error_inner(&metadata, error_message.data, error);
        return 0;
    }
    
    return 1;
}

int check_block_statement_soundness(struct statement *s,
                                    struct global_context *global_context,
                                    struct context *context,
                                    struct error *error)
{
    assert(s->kind == BLOCK_STATEMENT);
    for (size_t i = 0; i < s->statements->size; i++) {
        if (!check_statement_soundness(&s->statements->data[i],
                                       global_context,
                                       context,
                                       error))
        {
            return 0;
        }
    }

    return 1;
}

int check_action_statement_soundness(struct statement *s,
                                     struct global_context *global_context,
                                     struct context *context,
                                     struct error *error)
{
    assert(s->kind == ACTION_STATEMENT);
    struct list_char error_message = list_create(char, 100);
    struct list_scoped_variable *scoped_variables =
        &lut_get(&context->statement_scope_lookup, s->id).scoped_variables;

    if (!check_expression_soundness(&s->expression,
                                    global_context,
                                    scoped_variables,
                                    &error_message))
    {
        struct statement_metadata metadata =
            lut_get(&global_context->metadata_lookup, s->id);
        add_error_inner(&metadata, error_message.data, error);
        return 0;
    }
    return 1;
}

int check_while_statement_soundness(struct statement *s,
                                    struct global_context *global_context,
                                    struct context *context,
                                    struct error *error)
{
    assert(s->kind == WHILE_LOOP_STATEMENT);
    struct while_loop_statement *while_statement = &s->while_loop_statement;
    struct list_char error_message = list_create(char, 100);
    struct list_scoped_variable *scoped_variables =
        &lut_get(&context->statement_scope_lookup, s->id).scoped_variables;

    if (!check_expression_soundness(&while_statement->condition,
                                    global_context,
                                    scoped_variables,
                                    &error_message))
    {
        struct statement_metadata metadata =
            lut_get(&global_context->metadata_lookup, s->id);
        add_error_inner(&metadata, error_message.data, error);
        return 0;
    }
    
    if (!check_statement_soundness(while_statement->do_statement,
                                   global_context,
                                   context,
                                   error))
    {
        return 0;
    }

    return 1;
}

int check_statement_soundness(struct statement *s,
                              struct global_context *global_context,
                              struct context *context,
                              struct error *error)
{
    switch (s->kind) {
        case RETURN_STATEMENT:
            return check_return_statement_soundness(s, global_context, context, error);
        case BINDING_STATEMENT:
            return check_binding_statement_soundness(s, global_context, context, error);
        case IF_STATEMENT:
            return check_if_statement_soundness(s, global_context, context, error);
        case BLOCK_STATEMENT:
            return check_block_statement_soundness(s, global_context, context, error);
        case ACTION_STATEMENT:
            return check_action_statement_soundness(s, global_context, context, error);
        case WHILE_LOOP_STATEMENT:
            return check_while_statement_soundness(s, global_context, context, error);
        case BREAK_STATEMENT:
        case SWITCH_STATEMENT:
            return 0;
        case C_BLOCK_STATEMENT:
            return 1;
        case TYPE_DECLARATION_STATEMENT:
            UNREACHABLE("top level statements shouldn't be here.");
        }

    UNREACHABLE("dropped out of switch in check_statement_soundness.");
}


int soundness_check(struct parsed_file *parsed_file,
                    struct context *context,
                    struct error *error)
{
    for (size_t i = 0; i < parsed_file->statements.size; i++) {
        struct statement *s = &parsed_file->statements.data[i];
        switch (s->kind) {
            case TYPE_DECLARATION_STATEMENT:
            {
                switch (s->type_declaration.type.kind) {
                    case TY_FUNCTION:
                    {
                        if (!check_fn_soundness(&s->type_declaration,
                                                &parsed_file->global_context,
                                                context,
                                                error))
                        {
                            return 0;
                        }
                        break;
                    }
                    case TY_STRUCT:
                    {
                        struct list_char error_message = list_create(char, 100);
                        if (!check_struct_soundness(&s->type_declaration.type,
                                                    &parsed_file->global_context,
                                                    &error_message))
                        {
                            struct statement_metadata metadata =
                                lut_get(&parsed_file->global_context.metadata_lookup, s->id);
                            add_error_inner(&metadata, error_message.data, error);
                            return 0;
                        }
                        break;
                    }
                    case TY_ENUM:
                    {
                        struct list_char error_message = list_create(char, 100);
                        if (!check_enum_soundness(s->type_declaration.type.enum_type,
                                                  &parsed_file->global_context,
                                                  error))
                        {
                            struct statement_metadata metadata =
                                lut_get(&parsed_file->global_context.metadata_lookup, s->id);
                            add_error_inner(&metadata, error_message.data, error);
                            return 0;
                        }
                        break;
                    }
                    case TY_PRIMITIVE:
                        UNREACHABLE("TY_PRIMITIVE shouldn't have made it here via parsing.");
                    case TY_ANY:
                        UNREACHABLE("TY_ANY shouldn't have made it here via parsing.");
                }
                break;
            }
            default:
                UNREACHABLE("statement shouldn't have made it here via parsing.");
        }
    }

    return 1;
}
