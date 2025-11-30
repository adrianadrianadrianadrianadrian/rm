#include "ast.h"
#include "context.h"
#include "utils.h"
#include "error.h"
#include <assert.h>
#include <string.h>

int type_eq(struct type *l, struct type *r);
struct list_char show_type(struct type *ty);

static void add_error_inner(struct statement_metadata *metadata,
                            char *error_message,
                            struct error *out)
{
    add_error(metadata->row, metadata->col, metadata->file_name, out, error_message);
}

struct list_char type_mismatch_error(struct type *expected, struct type *actual)
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

int type_eq(struct type *l, struct type *r) {
    assert(l);
    assert(r);
    assert(l->kind);
    assert(r->kind);

    if (l->kind != r->kind) {
        return 0;
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

int expression_is_boolean(struct expression *e,
                          struct global_context *global_context,
                          struct list_scoped_variable *scoped_variables)
{
    switch (e->kind) {
        case UNARY_EXPRESSION:
        {
            switch (e->unary.unary_operator) {
                case BANG_UNARY:
                    return expression_is_boolean(e->unary.expression, global_context, scoped_variables);
                default:
                    return 0;
            }
        }
        case LITERAL_EXPRESSION:
        {
            switch (e->literal.kind) {
                case LITERAL_BOOLEAN:
                    return 1;
                default:
                    return 0;
            }
        }
        case BINARY_EXPRESSION:
        {
            switch (e->binary.binary_op) {
                case OR_BINARY:
                case AND_BINARY:
                case EQUAL_TO_BINARY:
                {
                    return expression_is_boolean(e->binary.l, global_context, scoped_variables)
                        || expression_is_boolean(e->binary.r, global_context, scoped_variables);
                }
                default:
                    return 0;
            }
        }
        case GROUP_EXPRESSION:
            return expression_is_boolean(e->grouped, global_context, scoped_variables);
        case FUNCTION_EXPRESSION:
        case VOID_EXPRESSION:
            return 0;
    }
    
    UNREACHABLE("dropped out of expression_is_boolean switch");
}

int binding_statement_check(struct statement_context *sc, struct error *error)
{
    assert(sc->kind == BINDING_STATEMENT);
    struct binding_statement_context *s = &sc->binding_statement;

    if (!s->binding_statement->has_type && s->inferred_type == NULL) {
        struct list_char message = list_create(char, 100);
        append_list_char_slice(&message, "type annotations needed for `");
        append_list_char_slice(&message, s->binding_statement->variable_name.data);
        append_list_char_slice(&message, "`.");
        add_error_inner(sc->metadata, message.data, error);
        return 0;
    }
    
    if (s->binding_statement->has_type && s->inferred_type == NULL)
    {
        // TODO: I think inferred should always be non null. Returning true always for now.
        //add_error_inner(sc->metadata, "unable to infer type.", error);
        return 1;
    }
    
    if (s->binding_statement->has_type
        && !type_eq(s->inferred_type, &s->binding_statement->variable_type))
    {
        add_error_inner(sc->metadata,
                        type_mismatch_error(&s->binding_statement->variable_type, s->inferred_type).data,
                        error);
        return 0;
    }

    return 1;
}

void all_return_statements_inner(struct statement_context *s_ctx, struct list_statement_context *out)
{
    if (s_ctx == NULL) {
        return;
    }

    switch (s_ctx->kind) {
        case RETURN_STATEMENT:
        {
            list_append(out, *s_ctx);
            return;
        }
        case IF_STATEMENT:
        {
            all_return_statements_inner(s_ctx->if_statement_context.success_statement, out);
            all_return_statements_inner(s_ctx->if_statement_context.else_statement, out);
            return;
        }
        case BLOCK_STATEMENT:
        {
            for (size_t i = 0; i < s_ctx->block_statements->size; i++) {
                all_return_statements_inner(&s_ctx->block_statements->data[i], out);
            }
            return;
        }
        case WHILE_LOOP_STATEMENT:
        {
            all_return_statements_inner(s_ctx->while_loop_statement.do_statement, out);
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
        case INCLUDE_STATEMENT:
            return;
    }
    
    UNREACHABLE("`all_return_statements_inner` fell out of it's switch.");
}

struct list_statement_context all_return_statements(struct statement_context *s_ctx)
{
    struct list_statement_context output = list_create(statement_context, 10);
    all_return_statements_inner(s_ctx, &output);
    return output;
}

int type_check_single(struct statement_context *s, struct error *error)
{
    switch (s->kind) {
        case BINDING_STATEMENT:
            return binding_statement_check(s, error);
        case TYPE_DECLARATION_STATEMENT:
        {
            if (s->type_declaration.type.kind != TY_FUNCTION) return 1;
            struct type *expected_return_type = s->type_declaration.type.function_type.return_type;
            struct list_statement_context *body = s->type_declaration.statements;

            for (size_t i = 0; i < body->size; i++) {
                struct list_statement_context return_statements = all_return_statements(&body->data[i]);
                if (return_statements.size) {
                    for (size_t j = 0; j < return_statements.size; j++) {
                        struct statement_context *this = &return_statements.data[j];
                        struct type *actual = this->return_statement.inferred_return_type;

                        if (!type_eq(expected_return_type, actual)) {
                            add_error_inner(this->metadata,
                                            type_mismatch_error(expected_return_type, actual).data,
                                            error);
                            return 0;
                        }
                    }
                } else {
                    if (!type_check_single(&s->type_declaration.statements->data[i], error)) return 0;
                }
            }
            return 1;
        }
        case IF_STATEMENT:
        case SWITCH_STATEMENT:
        case BLOCK_STATEMENT:
        case WHILE_LOOP_STATEMENT:
        case ACTION_STATEMENT:
        case RETURN_STATEMENT:
        // cases to ignore
        case INCLUDE_STATEMENT:
        case BREAK_STATEMENT:
            return 1;
    }

    UNREACHABLE("type_check_single dropped out of a switch on all kinds of statements.");
}

int type_check(struct list_statement_context statements, struct error *error) {
    for (size_t i = 0; i < statements.size; i++) {
        if (!type_check_single(&statements.data[i], error))    return 0;
    }
    return 1;
}

struct list_char show_type(struct type *ty) {
    struct list_char output = list_create(char, 10);
    if (ty == NULL) {
        return output;
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
