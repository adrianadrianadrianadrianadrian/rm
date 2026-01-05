#include <assert.h>
#include <string.h>
#include "ast.h"
#include "context.h"
#include "utils.h"
#include <string.h>
#include "type_checker.h"
#include "error.h"

int find_struct_definition(struct global_context *c,
                           struct list_char *struct_name,
                           struct type *out)
{
    for (size_t i = 0; i < c->data_types.size; i++) {
        struct type this = c->data_types.data[i];
        if (this.kind != TY_STRUCT) {
            continue;
        }

        if (list_char_eq(struct_name, this.name)) {
            *out = this;
            return 1;
        }
    }
    
    UNREACHABLE("struct does not exist.");
}

int find_enum_definition(struct global_context *c,
                           struct list_char *enum_name,
                           struct type *out)
{
    for (size_t i = 0; i < c->data_types.size; i++) {
        struct type this = c->data_types.data[i];
        if (this.kind != TY_ENUM) {
            continue;
        }

        if (list_char_eq(enum_name, this.name)) {
            *out = this;
            return 1;
        }
    }
    
    UNREACHABLE("enum does not exist.");
}

struct type *get_type(struct scoped_variable *variable) {
    struct type *output = NULL; 
    // variable->defined_type != NULL
    //     ? variable->defined_type
    //     : variable->inferred_type;
    assert(output);
    return output;
}

int get_scoped_variable_type(struct list_scoped_variable *scoped_variables,
                             struct global_context *c,
					         struct list_char ident_name,
							 struct type *out)
{
    for (size_t i = 0; i < scoped_variables->size; i++) {
        struct scoped_variable scoped = scoped_variables->data[i];
        if (list_char_eq(&scoped.name, &ident_name)) {
            struct type *ty = get_type(&scoped);
            if (ty == NULL) {
                return 0;
            }

            *out = *get_type(&scoped);
            return 1;
        }
    }
    
    for (size_t i = 0; i < c->fn_types.size; i++) {
        struct type *fn = &c->fn_types.data[i];
        if (list_char_eq(fn->name, &ident_name)) {
            *out = *fn;
            return 1;
        }
    }

	return 0;
}


int infer_literal_expression_type(struct literal_expression *e,
                                  struct global_context *global_context,
                                  struct list_scoped_variable *scoped_variables,
                                  struct type *out,
                                  struct list_char *error)
{
    switch (e->kind) {
        case LITERAL_STRUCT:
            return find_struct_definition(global_context, e->struct_enum.name, out);
        case LITERAL_ENUM:
            return find_enum_definition(global_context, e->struct_enum.name, out);
        case LITERAL_NAME:
        {
            for (size_t i = 0; i < scoped_variables->size; i++) {
                if (list_char_eq(e->name, &scoped_variables->data[i].name)) {
                    struct type *t = get_type(&scoped_variables->data[i]);
                    // TODO: enums
                    if (t->kind == TY_STRUCT) {
                        find_struct_definition(global_context, t->name, t);
                    }
                    *out = *t;
                    return 1;
                }
            }

            for (size_t i = 0; i < global_context->data_types.size; i++) {
                if (list_char_eq(e->name, global_context->data_types.data[i].name)) {
                    *out = global_context->data_types.data[i];
                    return 1;
                }
            }

            for (size_t i = 0; i < global_context->fn_types.size; i++) {
                if (list_char_eq(e->name, global_context->fn_types.data[i].name)) {
                    *out = global_context->fn_types.data[i];
                    return 1;
                }
            }

            UNREACHABLE("cannot find literal name.");
        }
        case LITERAL_BOOLEAN:
        {
            *out = (struct type) {
                .kind = TY_PRIMITIVE,
                .primitive_type = BOOL
            };
            return 1;
        }
        case LITERAL_CHAR:
        {
            *out = (struct type) {
                .kind = TY_PRIMITIVE,
                .primitive_type = U8
            };
            return 1;
        }
        case LITERAL_NUMERIC:
        {
            *out = (struct type) {
                .kind = TY_PRIMITIVE,
                .primitive_type = I32
            };
            return 1;
        }
        case LITERAL_STR:
        {
            struct list_type_modifier modifiers = list_create(type_modifier, 1);
            struct type_modifier array = (struct type_modifier) {
                .kind = ARRAY_MODIFIER_KIND,
                .array_modifier = (struct array_type_modifier) {
                    .literally_sized = 1,
                    .literal_size = e->str->size
                }
            };
            list_append(&modifiers, array);
            
            *out = (struct type) {
                .kind = TY_PRIMITIVE,
                .primitive_type = U8,
                .modifiers = modifiers
            };
            return 1;
        }
        case LITERAL_HOLE:
        case LITERAL_NULL:
            return 1;
    }

    UNREACHABLE("infer_literal_expression_type: dropped out of switch.");
}

int infer_function_type(struct function_type *matched_fn,
                        struct global_context *global_context,
                        size_t value_count,
                        struct type *out)
{
    if (matched_fn->params.size < value_count || value_count == 0) {
        return 0;
    }
    
    if (matched_fn->params.size == value_count) {
        if (matched_fn->return_type->kind == TY_STRUCT) {
            return find_struct_definition(global_context, matched_fn->return_type->name, out);
        }

        *out = *matched_fn->return_type;
        return 1;
    }
    
    struct type *inferred = malloc(sizeof(*inferred));
    struct list_key_type_pair params = list_create(key_type_pair, matched_fn->params.size - value_count);
    for (size_t i = value_count; i < matched_fn->params.size; i++) {
        list_append(&params, matched_fn->params.data[i]);
    }

    *out = (struct type) {
        .kind = TY_FUNCTION,
        .function_type = (struct function_type) {
            .params = params,
            .return_type = matched_fn->return_type
        }
    };
    return 1;
}

int get_field_type(struct list_key_type_pair *pairs,
                   struct list_char *field_name,
                   struct global_context *global_context,
                   struct type *out)
{
    for (size_t i = 0; i < pairs->size; i++) {
        if (list_char_eq(field_name, &pairs->data[i].field_name)) {
            struct type *found = pairs->data[i].field_type;

            if (found->kind == TY_STRUCT && found->struct_type.predefined) {
                find_struct_definition(global_context, found->name, found);
            } else if (found->kind == TY_ENUM && found->enum_type.predefined) {
                find_enum_definition(global_context, found->name, found);
            }
            
            *out = *found;
            return 1;
        }
    }
    
    return 0;
}

int infer_expression_type(struct expression *e,
                          struct global_context *global_context,
                          struct list_scoped_variable *scoped_variables,
                          struct type *out,
                          struct list_char *error)
{
    switch (e->kind) {
        case LITERAL_EXPRESSION:
            return infer_literal_expression_type(&e->literal, global_context, scoped_variables, out, error);
        case UNARY_EXPRESSION:
            return infer_expression_type(e->unary.expression, global_context, scoped_variables, out, error);
        case BINARY_EXPRESSION:
        {
            struct type left = {0};
            struct type right = {0};
            if (!infer_expression_type(e->binary.l, global_context, scoped_variables, &left, error)) return 0;
            if (!infer_expression_type(e->binary.r, global_context, scoped_variables, &right, error)) return 0;
            // TODO: expression type context on all levels instead of this, then check in type_checker.
            // if (!type_eq(&left, &right)) {
            //     append_list_char_slice(error, "binary expression branches must have the same type.");
            //     return 0;
            // }
            *out = left;
            return 1;
        }
        case GROUP_EXPRESSION:
            return infer_expression_type(e->grouped, global_context, scoped_variables, out, error);
        case FUNCTION_EXPRESSION:
        {
            size_t value_count = e->function.params->size;
            
            for (size_t i = 0; i < global_context->fn_types.size; i++) {
                struct function_type global_fn = global_context->fn_types.data[i].function_type;
                if (list_char_eq(e->function.function_name, global_context->fn_types.data[i].name)) {
                    return infer_function_type(&global_fn, global_context, value_count, out);
                }
            }

            for (size_t i = 0; i < scoped_variables->size; i++) {
                struct type *fn = get_type(&scoped_variables->data[i]);
                if (fn->kind == TY_FUNCTION) {
                    if (list_char_eq(e->function.function_name, &scoped_variables->data[i].name)) {
                        return infer_function_type(&fn->function_type, global_context, value_count, out);
                    }
                }
            }

            UNREACHABLE("function does not exist in global_context.");
        }
        case MEMBER_ACCESS_EXPRESSION:
        {
            struct type accessed = {0};
            if (!infer_expression_type(e->member_access.accessed,
                                       global_context,
                                       scoped_variables,
                                       &accessed,
                                       error))
            {
                return 0;
            }
            
            // TODO: enums
            if (accessed.kind != TY_STRUCT) {
                append_list_char_slice(error, "can only access fields of structs.");
                return 0;
            }

            if (!get_field_type(&accessed.struct_type.pairs,
                                e->member_access.member_name,
                                global_context,
                                out))
            {
                append_list_char_slice(error, "field `");
                append_list_char_slice(error, e->member_access.member_name->data);
                append_list_char_slice(error, "` does not exist on `struct ");
                append_list_char_slice(error, accessed.name->data);
                append_list_char_slice(error, "`.");
                return 0;
            }

            return 1;
        }
        case VOID_EXPRESSION:
        {
            *out = (struct type) {
                .kind = TY_PRIMITIVE,
                .primitive_type = VOID
            };
            return 1;
        }
    }

    UNREACHABLE("infer_expression_type: fell out of switch case.");
}


struct list_char show_type(struct type *ty);
int type_check_single(struct statement_context *s, struct error *error);
int type_eq(struct type *l, struct type *r);

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

int binding_statement_check(struct statement_context *sc, struct error *error)
{
    // assert(sc->kind == BINDING_STATEMENT);
    // struct binding_statement_context *s = &sc->binding_statement;
    // assert(s->binding_statement->has_type && s->inferred_type.kind != 0);
    //
    // if (!s->binding_statement->has_type && s->inferred_type.kind == 0) {
    //     struct list_char message = list_create(char, 100);
    //     append_list_char_slice(&message, "type annotations needed for `");
    //     append_list_char_slice(&message, s->binding_statement->variable_name.data);
    //     append_list_char_slice(&message, "`.");
    //     add_error_inner(sc->metadata, message.data, error);
    //     return 0;
    // }
    // 
    // if (s->binding_statement->has_type
    //     && !type_eq(&s->inferred_type, &s->binding_statement->variable_type))
    // {
    //     add_error_inner(sc->metadata,
    //                     type_mismatch_generic_error(&s->binding_statement->variable_type, &s->inferred_type).data,
    //                     error);
    //     return 0;
    // }

    return 1;
}

void all_return_statements_inner(struct statement_context *s_ctx, struct list_statement_context *out)
{
    assert(s_ctx != NULL);
    switch (s_ctx->kind) {
        case RETURN_STATEMENT:
        {
            list_append(out, *s_ctx);
            return;
        }
        case IF_STATEMENT:
        {
            all_return_statements_inner(s_ctx->if_statement_context.success_statement, out);
            if (s_ctx->if_statement_context.else_statement != NULL)  {
                all_return_statements_inner(s_ctx->if_statement_context.else_statement, out);
            }
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
        case C_BLOCK_STATEMENT:
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

int type_check_action_statement(struct statement_context *s, struct error *error)
{
    assert(s->kind == ACTION_STATEMENT);
    if (s->expression->kind != FUNCTION_EXPRESSION) {
        return 1;
    }

    struct function_expression *fn_expr = &s->expression->function;
    struct type fn = {0};
    if (!find_function_definition(fn_expr->function_name, s->global_context, &fn)) return 0;

    // assumption: fn's list of name:type is ordered how it's defined in the source code,
    // and the params list of expressions is ordered how it's written in the source code.
    assert(fn_expr->params->size <= fn.function_type.params.size);
    for (size_t i = 0; i < fn_expr->params->size; i++) {
        struct type param_type = {0};
        struct list_char error_message = list_create(char, 100);
        // TODO: do this as part of contextualisation
        if (!infer_expression_type(&fn_expr->params->data[i],
                                   s->global_context,
                                   &s->scoped_variables,
                                   &param_type,
                                   &error_message))
        {
            add_error_inner(s->metadata, error_message.data, error);
            return 0;
        }

        struct type *expected = fn.function_type.params.data[i].field_type;
        struct type *actual = &param_type;
        if (!type_eq(actual, expected)) {
            append_list_char_slice(&error_message, "mismatch types; expected `");
            append_list_char_slice(&error_message, show_type(expected).data);
            append_list_char_slice(&error_message, "` for parameter '");
            append_list_char_slice(&error_message, fn.function_type.params.data[i].field_name.data);
            append_list_char_slice(&error_message, "' but got `");
            append_list_char_slice(&error_message, show_type(actual).data);
            append_list_char_slice(&error_message, "` (in function '");
            append_list_char_slice(&error_message, fn.name->data);
            append_list_char_slice(&error_message, "').");
            add_error_inner(s->metadata, error_message.data, error);
            return 0;
        }
    }
    return 1;
}

int type_check_single(struct statement_context *s, struct error *error)
{
    // switch (s->kind) {
    //     case BINDING_STATEMENT:
    //         return binding_statement_check(s, error);
    //     case TYPE_DECLARATION_STATEMENT:
    //     {
    //         if (s->type_declaration.type.kind != TY_FUNCTION) return 1;
    //         struct type *expected_return_type = s->type_declaration.type.function_type.return_type;
    //         struct list_statement_context *body = s->type_declaration.statements;
    //
    //         for (size_t i = 0; i < body->size; i++) {
    //             struct list_statement_context return_statements = all_return_statements(&body->data[i]);
    //             if (return_statements.size) {
    //                 for (size_t j = 0; j < return_statements.size; j++) {
    //                     struct statement_context *this = &return_statements.data[j];
    //                     // TODO
    //                     struct type *actual = NULL; //&this->return_statement.e.type;
    //
    //                     if (!type_eq(expected_return_type, actual)) {
    //                         add_error_inner(this->metadata,
    //                                         type_mismatch_generic_error(expected_return_type, actual).data,
    //                                         error);
    //                         return 0;
    //                     }
    //                 }
    //             } else {
    //                 if (!type_check_single(&s->type_declaration.statements->data[i], error)) return 0;
    //             }
    //         }
    //         return 1;
    //     }
    //     case IF_STATEMENT:
    //     {
    //         struct if_statement_context *ctx = &s->if_statement_context;
    //         if (!is_boolean(&ctx->condition.type))
    //         {
    //             add_error_inner(s->metadata, "the condition of an if statement must be a boolean.", error);
    //             return 0;
    //         }
    //         
    //         if (!type_check_single(ctx->success_statement, error)) return 0;
    //         if (ctx->else_statement != NULL
    //             && !type_check_single(ctx->else_statement, error)) return 0;
    //         
    //         return 1;
    //     }
    //     case WHILE_LOOP_STATEMENT:
    //     {
    //         struct while_loop_statement_context *ctx = &s->while_loop_statement;
    //         if (!is_boolean(&ctx->condition.type))
    //         {
    //             add_error_inner(s->metadata, "the condition of a while loop must be a boolean.", error);
    //             return 0;
    //         }
    //         
    //         if (!type_check_single(ctx->do_statement, error)) return 0;
    //         return 1;
    //     }
    //     case BLOCK_STATEMENT:
    //     {
    //         for (size_t i = 0; i < s->block_statements->size; i++) {
    //             if (!type_check_single(&s->block_statements->data[i], error)) return 0;
    //         }
    //         return 1;
    //     }
    //     case ACTION_STATEMENT:
    //         return type_check_action_statement(s, error);
    //     case SWITCH_STATEMENT:
    //     {
    //         TODO("switch statement type check");
    //     }
    //     case RETURN_STATEMENT:
    //     case BREAK_STATEMENT:
    //     case C_BLOCK_STATEMENT:
    //         return 1;
    // }
    //
    // UNREACHABLE("type_check_single dropped out of a switch on all kinds of statements.");
}

int type_check(struct list_statement_context statements, struct error *error) {
    for (size_t i = 0; i < statements.size; i++) {
        if (!type_check_single(&statements.data[i], error)) return 0;
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
