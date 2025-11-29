#include "ast.h"
#include "context.h"
#include "utils.h"
#include "error.h"
#include <assert.h>

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

struct type *get_type(struct scoped_variable *variable) {
    return variable->defined_type != NULL ? variable->defined_type : variable->inferred_type;
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

int find_struct_definition(struct global_context *c,
                           struct list_char *struct_name,
                           struct type *out,
                           struct list_char *err)
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
    
    append_list_char_slice(err, "struct `");
    append_list_char_slice(err, struct_name->data);
    append_list_char_slice(err, "` does not exist.");
    return 0;
}

int find_enum_definition(struct global_context *c,
                           struct list_char *enum_name,
                           struct type *out,
                           struct list_char *err)
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
    
    append_list_char_slice(err, "enum `");
    append_list_char_slice(err, enum_name->data);
    append_list_char_slice(err, "` does not exist.");
    return 0;
}

int infer_field_type(struct struct_type s,
                     struct expression *e,
                     struct global_context *c,
                     struct list_char *err,
                     struct type *out)
{
    if (e->kind == LITERAL_EXPRESSION && e->literal.kind == LITERAL_NAME) {
        for (size_t i = 0; i < s.pairs.size; i++) {
            if (list_char_eq(e->literal.name, &s.pairs.data[i].field_name)) {
                *out = *s.pairs.data[i].field_type;
                return 1;
            }
        }
        
        append_list_char_slice(err, "no type found.");
        return 0;
    }

    if (e->kind == BINARY_EXPRESSION) {
        if (e->binary.l->kind != LITERAL_EXPRESSION && e->binary.l->literal.kind != LITERAL_NAME) {
            append_list_char_slice(err, "must be a valid field name");
            return 0;
        }
        
        for (size_t i = 0; i < s.pairs.size; i++) {
            if (list_char_eq(e->binary.l->literal.name, &s.pairs.data[i].field_name)) {
                if (s.pairs.data[i].field_type->kind != TY_STRUCT) {
                    append_list_char_slice(err, "cannot index into a primitive type");
                    return 0;
                }

                struct type *field_type = s.pairs.data[i].field_type;
                struct type complete_type = {0};
                if (find_struct_definition(c, field_type->name, &complete_type, err))
                {
                    return infer_field_type(complete_type.struct_type, e->binary.r, c, err, out);
                }
                
                append_list_char_slice(err, "unable to infer field type");
                return 0;
            } 
        }
    }

    append_list_char_slice(err, "woops, I broke.");
    return 0;
}

int infer_function_type(struct function_type *matched_fn,
                        size_t value_count,
                        struct type *out)
{
    if (matched_fn->params.size < value_count || value_count == 0) {
        return 0;
    }
    
    if (matched_fn->params.size == value_count) {
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


int infer_type(struct expression *e,
               struct list_scoped_variable *scoped_variables,
               struct global_context *c,
               struct type *out,
               struct list_char *err)
{
    switch (e->kind) {
        case LITERAL_EXPRESSION:
        {
            switch (e->literal.kind) {
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
                case LITERAL_STRUCT:
                {
                    return find_struct_definition(c, e->literal.struct_enum.name, out, err);
                }
                case LITERAL_ENUM:
                {
                    return find_enum_definition(c, e->literal.struct_enum.name, out, err);
                }
                case LITERAL_STR:
                case LITERAL_NUMERIC:
                case LITERAL_HOLE:
                case LITERAL_NULL:
                    return 0;
                case LITERAL_NAME:
					return get_scoped_variable_type(scoped_variables, c, *e->literal.name, out);
            }
        }
        case UNARY_EXPRESSION:
		{
			return infer_type(e->unary.expression, scoped_variables, c, out, err);
		}
        case BINARY_EXPRESSION:
        {
            // TODO recursive inferring
            if (e->binary.binary_op == DOT_BINARY)
            {
                struct type l_type = {0};
                if (infer_type(e->binary.l, scoped_variables, c, &l_type, err))
                {
                    if (l_type.kind == TY_STRUCT)
                    {
                        struct type complete = {0};
                        if (find_struct_definition(c, l_type.name, &complete, err)) {
                            return infer_field_type(complete.struct_type, e->binary.r, c, err, out);
                        }
                        return 0;
                    }
                }
            } else {
                if (e->binary.l->kind == LITERAL_EXPRESSION && e->binary.l->literal.kind == LITERAL_NAME)
                {
					return get_scoped_variable_type(scoped_variables,
                                                    c,
                                                    *e->binary.l->literal.name,
                                                    out);
                }
                else if (e->binary.r->kind == LITERAL_EXPRESSION && e->binary.r->literal.kind == LITERAL_NAME)
                {
					return get_scoped_variable_type(scoped_variables,
                                                    c,
                                                    *e->binary.r->literal.name,
                                                    out);
                }
            }
    
            return 0;
        }
        case GROUP_EXPRESSION:
        {
            return infer_type(e->grouped, scoped_variables, c, out, err);
        }
        case FUNCTION_EXPRESSION:
        {
            size_t value_count = e->function.params->size;
            
            for (size_t i = 0; i < c->fn_types.size; i++) {
                struct function_type global_fn = c->fn_types.data[i].function_type;
                if (list_char_eq(e->function.function_name, c->fn_types.data[i].name)) {
                    return infer_function_type(&global_fn, value_count, out);
                }
            }

            for (size_t i = 0; i < scoped_variables->size; i++) {
                struct type *fn = get_type(&scoped_variables->data[i]);
                if (fn->kind == TY_FUNCTION) {
                    if (list_char_eq(e->function.function_name, &scoped_variables->data[i].name)) {
                        return infer_function_type(&fn->function_type, value_count, out);
                    }
                }
            }
            return 0;
        }
        case VOID_EXPRESSION:
            return 0;
    }
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
            switch (s->type_declaration.type.kind)
            {
                case TY_FUNCTION:
                {
                    struct type *expected_return_type = s->type_declaration.type.function_type.return_type;
                    struct list_statement_context *body = s->type_declaration.statements;

                    for (size_t i = 0; i < body->size; i++) {
                        struct list_statement_context return_statements = all_return_statements(&body->data[i]);
                        if (return_statements.size) {
                            for (size_t j = 0; j < return_statements.size; j++) {
                                struct statement_context *this = &return_statements.data[j];
                                struct type *actual = this->return_statement.inferred_return_type;

                                if (actual == NULL) {
                                    // TODO: ties into the above mentioned todo regarding inferred types.
                                    continue;
                                }

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
                    break;
                }
                case TY_ENUM:
                case TY_STRUCT:
                    break;
                case TY_PRIMITIVE:
                    UNREACHABLE("type_check_single::TYPE_DECLARATION_STATEMENT::TY_PRIMITIVE");
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

int infer_expression_type(struct expression *e,
                          struct global_context *global_context,
                          struct list_scoped_variable *scoped_variables,
                          struct type *out,
                          struct error *error)
{
    return 1;
}

int add_type_information(struct statement_context *statement, struct error *error)
{
    switch (statement->kind) {
        case ACTION_STATEMENT:
        {
            struct type *inferred_type = malloc(sizeof(*inferred_type));
            if (!infer_expression_type(statement->action_statement_context.e,
                                       statement->global_context,
                                       &statement->scoped_variables,
                                       inferred_type,
                                       error))
            {
                return 0;
            }
            
            statement->action_statement_context.inferred_expression_type = inferred_type;
            return 1;
        }
        case RETURN_STATEMENT:
        {
            struct type *inferred_type = malloc(sizeof(*inferred_type));
            if (!infer_expression_type(statement->return_statement.e,
                                       statement->global_context,
                                       &statement->scoped_variables,
                                       inferred_type,
                                       error))
            {
                return 0;
            }
            
            statement->return_statement.inferred_return_type = inferred_type;
            return 1;
        }
        case BLOCK_STATEMENT:
        {
            for (size_t i = 0; i < statement->block_statements->size; i++) {
                if (!add_type_information(&statement->block_statements->data[i], error)) return 0;
            }
            return 1;
        }
        case TYPE_DECLARATION_STATEMENT:
        {
            if (statement->type_declaration.type.kind != TY_FUNCTION) return 0;
            for (size_t i = 0; i < statement->type_declaration.statements->size; i++) {
                if (!add_type_information(&statement->type_declaration.statements->data[i], error)) return 0;
            }
            return 1;
        }
        case BINDING_STATEMENT:
        {
            return 0;
        }
        case IF_STATEMENT:
        case WHILE_LOOP_STATEMENT:
        case SWITCH_STATEMENT:
            return 0;
        // ignore cases
        case BREAK_STATEMENT:
        case INCLUDE_STATEMENT:
            return 1;
    }

    UNREACHABLE("add_type_information: fell out of switch statement.");
}

int type_check(struct list_statement_context statements, struct error *error) {
    for (size_t i = 0; i < statements.size; i++) {
        if (!add_type_information(&statements.data[i], error)) return 0;
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
