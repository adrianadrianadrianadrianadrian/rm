#include <assert.h>
#include <string.h>
#include "ast.h"
#include "context.h"
#include "parser.h"
#include "utils.h"
#include <string.h>
#include "type_checker.h"
#include "error.h"

struct list_char show_type(struct type *ty);
int type_check_single(struct statement *s,
                      struct global_context *global_context,
                      struct context *context,
                      struct error *error);
int type_eq(struct type *l, struct type *r);
int type_check_expression(struct expression *e,
                          struct statement_metadata *statement_metadata,
                          struct global_context *global_context,
                          struct context *context,
                          struct error *error);

static void add_error_inner(struct statement_metadata *metadata,
                            char *error_message,
                            struct error *out)
{
    add_error(metadata->row, metadata->col, metadata->file_name, out, error_message);
}

struct list_char type_mismatch_generic_error(struct type *expected, struct type *actual)
{
    struct list_char output = list_create(char, 50);
    append_list_char_slice(&output, "mismatch types; expected `");
    append_list_char_slice(&output, show_type(expected).data);
    append_list_char_slice(&output, "` but got `");
    append_list_char_slice(&output, show_type(actual).data);
    append_list_char_slice(&output, "`.");
    return output;
}

int fn_type_eq(struct function_type *l, struct function_type *r) {
    if (l->params.size != r->params.size) {
        return 0;
    }
    
    if (!type_eq(l->return_type, r->return_type)) {
        return 0;
    }
        
    assert(l->params.size == r->params.size);
    for (size_t i = 0; i < l->params.size; i++) {
        if (!type_eq(l->params.data[i].field_type, r->params.data[i].field_type)) return 0;
    }

    return 1;
}

int type_modifier_eq(struct type_modifier *l, struct type_modifier *r)
{
    if (l->kind != r->kind) {
        return 0;
    }
    
    switch (l->kind) {
        case ARRAY_MODIFIER_KIND:
        {
            // TODO: reference based size
            if (l->array_modifier.literally_sized && r->array_modifier.literally_sized
                && l->array_modifier.literal_size != r->array_modifier.literal_size)
            {
                return 0;
            }
            return 1;
        }
        case POINTER_MODIFIER_KIND:
        case NULLABLE_MODIFIER_KIND:
        case MUTABLE_MODIFIER_KIND:
            return 1;
    }
    
    UNREACHABLE("type_modifier_eq fell out of switch.");
}

int type_eq(struct type *l, struct type *r) {
    assert(l);
    assert(r);
    assert(l->kind);
    assert(r->kind);

    if (l->kind != r->kind) {
        return 0;
    }
    
    if (l->modifiers.size != r->modifiers.size) {
        return 0;
    }
    
    for (size_t i = 0; i < l->modifiers.size; i++) {
        if (!type_modifier_eq(&l->modifiers.data[i], &r->modifiers.data[i])) return 0;
    }
    
    switch (l->kind) {
        case TY_PRIMITIVE:
            return l->primitive_type == r->primitive_type;
        case TY_STRUCT:
            return list_char_eq(l->name, r->name);
        case TY_FUNCTION:
            return fn_type_eq(&l->function_type, &r->function_type);
        case TY_ENUM:
            return list_char_eq(l->name, r->name);
    }

    return 0;
}

int is_boolean(struct type *ty)
{
    switch (ty->kind) {
        case TY_PRIMITIVE:
        {
            return ty->primitive_type == BOOL;
        }
        default:
            return 0;
    }
}

int binding_statement_check(struct statement *s,
                            struct global_context *global_context,
                            struct context *context,
                            struct error *error)
{
    assert(s->kind == BINDING_STATEMENT);
    struct statement_metadata metadata =
        lut_get(&global_context->metadata_lookup, s->id);

    if (!type_check_expression(&s->binding_statement.value,
                               &metadata,
                               global_context,
                               context,
                               error))
    {
        return 0;
    }

    if (s->binding_statement.has_type)
    {
        struct type actual_type =
            lut_get(&context->expression_type_lookup, s->binding_statement.value.id);
        // TODO: need to make sure we get the full type for non primitive annotations
        if (!type_eq(&s->binding_statement.variable_type, &actual_type)) {
            add_error_inner(&metadata,
                            type_mismatch_generic_error(&s->binding_statement.variable_type, 
                                                        &actual_type).data,
                            error);
        }
        return 0;
    }
    
    return 1;
}

void all_return_statements_inner(struct statement *s, struct list_statement *out)
{
    assert(s != NULL);
    switch (s->kind) {
        case RETURN_STATEMENT:
        {
            list_append(out, *s);
            return;
        }
        case IF_STATEMENT:
        {
            all_return_statements_inner(s->if_statement.success_statement, out);
            if (s->if_statement.else_statement != NULL)  {
                all_return_statements_inner(s->if_statement.else_statement, out);
            }
            return;
        }
        case BLOCK_STATEMENT:
        {
            for (size_t i = 0; i < s->statements->size; i++) {
                all_return_statements_inner(&s->statements->data[i], out);
            }
            return;
        }
        case WHILE_LOOP_STATEMENT:
        {
            all_return_statements_inner(s->while_loop_statement.do_statement, out);
            return;
        }
        case SWITCH_STATEMENT:
        {
            TODO("switch_statement is not part of the context yet.");
            return;
        }
        // cases to ignore
        case BINDING_STATEMENT:
        case ACTION_STATEMENT:
        case TYPE_DECLARATION_STATEMENT:
        case BREAK_STATEMENT:
        case C_BLOCK_STATEMENT:
            return;
    }

    UNREACHABLE("`all_return_statements_inner` fell out of it's switch.");
}

struct list_statement all_return_statements(struct statement *s)
{
    struct list_statement output = list_create(statement, 10);
    all_return_statements_inner(s, &output);
    return output;
}

int find_function_definition(struct list_char *function_name,
                             struct global_context *global_context,
                             struct type *out)
{
    for (size_t i = 0; i < global_context->fn_types.size; i++) {
        if (list_char_eq(function_name, global_context->fn_types.data[i].name)) {
            *out = global_context->fn_types.data[i];
            return 1;
        }
    }
    
    UNREACHABLE("all functions should exist in the global context by this point.");
}

int type_check_action_statement(struct statement *s,
                                struct global_context *global_context,
                                struct context *context,
                                struct error *error)
{
    assert(s->kind == ACTION_STATEMENT);
    if (s->expression.kind != FUNCTION_EXPRESSION) {
        return 1;
    }

    struct function_expression *fn_expr = &s->expression.function;
    struct type fn = {0};
    if (!find_function_definition(fn_expr->function_name, global_context, &fn)) return 0;

    // assumption: fn's list of name:type is ordered how it's defined in the source code,
    // and the params list of expressions is ordered how it's written in the source code.
    assert(fn_expr->params->size <= fn.function_type.params.size);
    for (size_t i = 0; i < fn_expr->params->size; i++) {
        // TODO: this expression should already have a type attached.
        struct expression *param_expr = &fn_expr->params->data[i];
        struct type *expected = fn.function_type.params.data[i].field_type;
        struct type actual_type =
            lut_get(&context->expression_type_lookup, param_expr->id);

        if (!type_eq(&actual_type, expected)) {
            struct list_char error_message = list_create(char, 100);
            append_list_char_slice(&error_message, "mismatch types; expected `");
            append_list_char_slice(&error_message, show_type(expected).data);
            append_list_char_slice(&error_message, "` for parameter '");
            append_list_char_slice(&error_message, fn.function_type.params.data[i].field_name.data);
            append_list_char_slice(&error_message, "' but got `");
            append_list_char_slice(&error_message, show_type(&actual_type).data);
            append_list_char_slice(&error_message, "` (in function '");
            append_list_char_slice(&error_message, fn.name->data);
            append_list_char_slice(&error_message, "').");
            struct statement_metadata metadata =
                lut_get(&global_context->metadata_lookup, s->id);
            add_error_inner(&metadata, error_message.data, error);
            return 0;
        }
    }
    return 1;
}

int unary_operator_allowed(enum unary_operator op,
                           struct type *expression_type,
                           struct list_char *error_message)
{
    switch (op) {
        case BANG_UNARY:
        {
            if (!is_boolean(expression_type)) {
                append_list_char_slice(error_message, "`!` can only be applied to boolean values.");
                return 0;
            }
            return 1;
        }
        case STAR_UNARY:
        case MINUS_UNARY:
        {
            return 0;
        }
    }
}

int type_check_function_expression(struct function_expression *fn,
                                   struct statement_metadata *statement_metadata,
                                   struct global_context *global_context,
                                   struct context *context,
                                   struct error *error)
{
    struct type fn_type = {0};
    find_function_definition(fn->function_name, global_context, &fn_type);
    assert(fn_type.kind == TY_FUNCTION);

    if (fn->params->size > fn_type.function_type.params.size) {
        add_error_inner(statement_metadata, "more params than fn allows.", error);
        return 0;
    }
        
    return 1;
    // for (size_t i = 0; i < fn_type.function_type.params.size; i++) {
    //     struct key_type_pair *param = &fn_type.function_type.params.data[i];
    // }
}

int type_check_expression(struct expression *e,
                          struct statement_metadata *statement_metadata,
                          struct global_context *global_context,
                          struct context *context,
                          struct error *error)
{
    struct list_char error_message = list_create(char, 100);
    struct type expression_type =
        lut_get(&context->expression_type_lookup, e->id);

    switch (e->kind) {
        case LITERAL_EXPRESSION:
        {
            return 1;
        }
        case UNARY_EXPRESSION:
        {
            if (!unary_operator_allowed(e->unary.unary_operator,
                                        &expression_type,
                                        &error_message))
            {
                add_error_inner(statement_metadata, error_message.data, error);
                return 0;
            }
            return type_check_expression(e->unary.expression,
                                         statement_metadata,
                                         global_context,
                                         context,
                                         error);
        }
        case BINARY_EXPRESSION:
        {
            TODO("binary fun");
        }
        case GROUP_EXPRESSION:
            return type_check_expression(e->grouped,
                                         statement_metadata,
                                         global_context,
                                         context,
                                         error);
        case FUNCTION_EXPRESSION:
            return type_check_function_expression(&e->function,
                                                  statement_metadata,
                                                  global_context,
                                                  context,
                                                  error);
        case VOID_EXPRESSION:
        case MEMBER_ACCESS_EXPRESSION:
            return 1;
    };
}

int type_check_single(struct statement *s,
                      struct global_context *global_context,
                      struct context *context,
                      struct error *error)
{
    switch (s->kind) {
        case BINDING_STATEMENT:
            return binding_statement_check(s, global_context, context, error);
        case TYPE_DECLARATION_STATEMENT:
        {
            if (s->type_declaration.type.kind != TY_FUNCTION) return 1;
            struct type *expected_return_type = s->type_declaration.type.function_type.return_type;
            struct list_statement *body = s->type_declaration.statements;

            for (size_t i = 0; i < body->size; i++) {
                struct list_statement return_statements = all_return_statements(&body->data[i]);
                if (return_statements.size) {
                    struct list_char error_message = list_create(char, 100);
                    for (size_t j = 0; j < return_statements.size; j++) {
                        struct statement *this = &return_statements.data[j];
                        assert(this->kind == RETURN_STATEMENT);
                        struct type actual_type =
                            lut_get(&context->expression_type_lookup, this->expression.id);
                        if (!type_eq(expected_return_type, &actual_type)) {
                            struct statement_metadata metadata =
                                lut_get(&global_context->metadata_lookup, s->id);
                            add_error_inner(&metadata,
                                            type_mismatch_generic_error(expected_return_type, &actual_type).data,
                                            error);
                            return 0;
                        }
                    }
                } else {
                    if (!type_check_single(&s->type_declaration.statements->data[i],
                                           global_context,
                                           context,
                                           error))
                    {
                        return 0;
                    }
                }
            }
            return 1;
        }
        case IF_STATEMENT:
        {
            struct if_statement *if_statement = &s->if_statement;
            struct type condition_type =
                lut_get(&context->expression_type_lookup, if_statement->condition.id);
            if (!is_boolean(&condition_type))
            {
                struct statement_metadata metadata =
                    lut_get(&global_context->metadata_lookup, s->id);
                add_error_inner(&metadata, "the condition of an if statement must be a boolean.", error);
                return 0;
            }
            if (!type_check_single(if_statement->success_statement, global_context, context, error)) return 0;
            if (if_statement->else_statement != NULL
                && !type_check_single(if_statement->else_statement, global_context, context, error)) return 0;
            return 1;
        }
        case WHILE_LOOP_STATEMENT:
        {
            struct while_loop_statement *while_statement = &s->while_loop_statement;
            struct type condition_type =
                lut_get(&context->expression_type_lookup, while_statement->condition.id);
            if (!is_boolean(&condition_type))
            {
                struct statement_metadata metadata =
                    lut_get(&global_context->metadata_lookup, s->id);
                add_error_inner(&metadata, "the condition of a while loop must be a boolean.", error);
                return 0;
            }
            if (!type_check_single(while_statement->do_statement, global_context, context, error)) return 0;
            return 1;
        }
        case BLOCK_STATEMENT:
        {
            for (size_t i = 0; i < s->statements->size; i++) {
                if (!type_check_single(&s->statements->data[i], global_context, context, error)) return 0;
            }
            return 1;
        }
        case ACTION_STATEMENT:
            return type_check_action_statement(s, global_context, context, error);
        case SWITCH_STATEMENT:
        {
            TODO("switch statement type check");
        }
        case RETURN_STATEMENT:
        case BREAK_STATEMENT:
        case C_BLOCK_STATEMENT:
            return 1;
    }

    UNREACHABLE("type_check_single dropped out of a switch on all kinds of statements.");
}

int type_check(struct parsed_file *parsed_file, struct context *context, struct error *error) {
    struct list_statement statements = parsed_file->statements;
    for (size_t i = 0; i < statements.size; i++) {
        if (!type_check_single(&statements.data[i], &parsed_file->global_context, context, error)) return 0;
    }
    return 1;
}

struct list_char show_modifier(struct type_modifier *m)
{
    struct list_char output = list_create(char, 10);
    switch (m->kind) {
        case POINTER_MODIFIER_KIND:
        {
            append_list_char_slice(&output, "*");
            break; 
        }
        case NULLABLE_MODIFIER_KIND:
        {
            append_list_char_slice(&output, "?");
            break; 
        }
        case MUTABLE_MODIFIER_KIND:
        {
            append_list_char_slice(&output, "mut ");
            break; 
        }
        case ARRAY_MODIFIER_KIND:
        {
            append_list_char_slice(&output, "[");
            if (m->array_modifier.literally_sized) {
                char tmp[1024] = {0}; // TODO don't do this...
                sprintf(tmp, "%d", m->array_modifier.literal_size);
                append_list_char_slice(&output, tmp);
            } else if (m->array_modifier.reference_sized) {
                append_list_char_slice(&output, m->array_modifier.reference_name->data);
            }
            append_list_char_slice(&output, "]");
            break; 
        }
    }

    return output;
}

struct list_char show_type(struct type *ty) {
    struct list_char output = list_create(char, 10);
    if (ty == NULL) {
        return output;
    }

    for (size_t i = 0; i < ty->modifiers.size; i++) {
        append_list_char_slice(&output, show_modifier(&ty->modifiers.data[i]).data);
    }

    switch (ty->kind) {
        case TY_PRIMITIVE:
        {
            switch (ty->primitive_type) {
                case VOID:
                    append_list_char_slice(&output, "void");
                    break;
                case BOOL:
                    append_list_char_slice(&output, "bool");
                    break;
                case U8:
                    append_list_char_slice(&output, "u8");
                    break;
                case I8:
                    append_list_char_slice(&output, "i8");
                    break;
                case I16:
                    append_list_char_slice(&output, "i16");
                    break;
                case U16:
                    append_list_char_slice(&output, "u16");
                    break;
                case I32:
                    append_list_char_slice(&output, "i32");
                    break;
                case U32:
                    append_list_char_slice(&output, "u32");
                    break;
                case I64:
                    append_list_char_slice(&output, "i64");
                    break;
                case U64:
                    append_list_char_slice(&output, "u64");
                    break;
                case USIZE:
                    append_list_char_slice(&output, "usize");
                    break;
                case F32:
                    append_list_char_slice(&output, "f32");
                    break;
                case F64:
                    append_list_char_slice(&output, "f64");
                    break;
            }
            break;
        }
        case TY_STRUCT:
        {
            append_list_char_slice(&output, "struct ");
            append_list_char_slice(&output, ty->name->data);
            break;
        }
        case TY_ENUM:
        {
            append_list_char_slice(&output, "enum ");
            append_list_char_slice(&output, ty->name->data);
            break;
        }
        case TY_FUNCTION:
        {
            append_list_char_slice(&output, "fn(");
            for (size_t i = 0; i < ty->function_type.params.size; i++) {
                struct list_char param_type = show_type(ty->function_type.params.data[i].field_type);
                append_list_char_slice(&output, param_type.data);
                if (i < ty->function_type.params.size - 1) {
                    append_list_char_slice(&output, ", ");
                }
            }
            append_list_char_slice(&output, ") -> ");
            struct list_char return_type = show_type(ty->function_type.return_type);
            append_list_char_slice(&output, return_type.data);
            break;
        }
    }

    append_list_char_slice(&output, "\0");
    return output;
}
