#include "ast.h"
#include "context.h"
#include "utils.h"
#include <assert.h>

int type_eq(struct type *l, struct type *r);
struct list_char show_type(struct type *ty);

struct type_check_error {
    struct list_char error_message;
    struct list_char suggestion;
};

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

int binding_statement_check(struct binding_statement_context *s, struct type_check_error *error)
{
    if (!s->binding_statement->has_type && s->inferred_type == NULL) {
        append_list_char_slice(&error->error_message, "type annotations needed.");
        return 0;
    }
    
    if (s->binding_statement->has_type && s->inferred_type == NULL)
    {
        // TODO check value is the type declared.
        return 1;
    }
    
    if (s->binding_statement->has_type
        && !type_eq(s->inferred_type, &s->binding_statement->variable_type))
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
        case TYPE_DECLARATION_STATEMENT:
        {
            switch (s->type_declaration.type.kind) {
                case TY_FUNCTION:
                {
                    for (size_t i = 0; i < s->type_declaration.statements->size; i++) {
                        struct statement_context s_ctx = s->type_declaration.statements->data[i];
                        if (s->type_declaration.statements->data[i].kind == RETURN_STATEMENT) {
                            // need to find all nested returns
                            // struct return_statement_context ctx = s_ctx.return_statement;
                            // if (!type_eq(ctx.inferred_return_type, s->type_declaration.type.function_type.return_type)) {
                            //     printf("return type is not correct...");
                            //     return 0;
                            // }
                        } else {
                            if (!type_check_single(&s->type_declaration.statements->data[i], error)) {
                                return 0;
                            }
                        }
                    }
                    break;
                }
                case TY_ENUM:
                case TY_STRUCT:
                case TY_PRIMITIVE:
                    break;
            }
            return 1;
        }
        case BREAK_STATEMENT:
        case SWITCH_STATEMENT:
        case INCLUDE_STATEMENT:
        case ACTION_STATEMENT:
        case IF_STATEMENT:
        case RETURN_STATEMENT:
        case BLOCK_STATEMENT:
        case WHILE_LOOP_STATEMENT:
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

struct list_char show_type(struct type *ty) {
    struct list_char output = list_create(char, 10);
    if (ty == NULL) {
        return output;
    }

    switch (ty->kind) {
        case TY_PRIMITIVE:
        {
            switch (ty->primitive_type) {
                case UNIT:
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
