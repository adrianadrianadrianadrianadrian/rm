#include "ast.h"
#include "utils.h"
#include <assert.h>
#include <string.h>
#include "utils.h"
#include "context.h"

#ifdef DEBUG_CONTEXT
void show_statement_context(struct statement_context *s);
#endif

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
                           struct context_error *err)
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
    
    append_list_char_slice(&err->message, "Struct does not exist.");
    return 0;
}

int find_enum_definition(struct global_context *c,
                           struct list_char struct_name,
                           struct type *out,
                           struct context_error *err)
{
    for (size_t i = 0; i < c->data_types.size; i++) {
        struct type this = c->data_types.data[i];
        if (this.kind != TY_ENUM) {
            continue;
        }

        if (list_char_eq(&struct_name, this.name)) {
            *out = this;
            return 1;
        }
    }
    
    append_list_char_slice(&err->message, "Struct does not exist.");
    return 0;
}

int infer_field_type(struct struct_type s,
                     struct expression *e,
                     struct global_context *c,
                     struct context_error *err,
                     struct type *out)
{
    if (e->kind == LITERAL_EXPRESSION && e->literal.kind == LITERAL_NAME) {
        for (size_t i = 0; i < s.pairs.size; i++) {
            if (list_char_eq(e->literal.name, &s.pairs.data[i].field_name)) {
                *out = *s.pairs.data[i].field_type;
                return 1;
            }
        }
        
        append_list_char_slice(&err->message, "no type found.");
        return 0;
    }

    if (e->kind == BINARY_EXPRESSION) {
        if (e->binary.l->kind != LITERAL_EXPRESSION && e->binary.l->literal.kind != LITERAL_NAME) {
            append_list_char_slice(&err->message, "Must be a valid field name");
            return 0;
        }
        
        for (size_t i = 0; i < s.pairs.size; i++) {
            if (list_char_eq(e->binary.l->literal.name, &s.pairs.data[i].field_name)) {
                if (s.pairs.data[i].field_type->kind != TY_STRUCT) {
                    append_list_char_slice(&err->message, "Cannot index into a primitive type");
                    return 0;
                }

                struct type *field_type = s.pairs.data[i].field_type;
                struct type complete_type = {0};
                if (find_struct_definition(c, field_type->name, &complete_type, err))
                {
                    return infer_field_type(complete_type.struct_type, e->binary.r, c, err, out);
                }
                
                append_list_char_slice(&err->message, "Unable to infer field type");
                return 0;
            } 
        }
    }

    append_list_char_slice(&err->message, "Not sure what happend");
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
               struct context_error *err)
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
                    return find_struct_definition(c, e->literal.struct_enum.name, out, err);
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
    }
}

int include_statement_global_context(struct statement *s,
                                     struct global_context *gc,
                                     struct context_error *context_error)
{
    // TODO
    return 1;
}

int type_declaration_global_context(struct statement *s,
                                    struct global_context *gc,
                                    struct context_error *context_error)
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
                            struct context_error *context_error)
{
    if (s->size == 0) {
        return 0;
    }

    for (size_t i = 0; i < s->size; i++) {
        struct statement *current = &s->data[i];
        switch (current->kind) {
            case TYPE_DECLARATION_STATEMENT:
                if (!type_declaration_global_context(current, out, context_error)) return 0;
                break;
            case INCLUDE_STATEMENT:
                if (!include_statement_global_context(current, out, context_error)) return 0;
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
        .inferred_type = c->binding_statement.inferred_type
    };
    
    list_append(scoped_variables, var);
}

int contextualise_statement(struct statement *s,
                            struct global_context *global_context,
                            struct list_scoped_variable *scoped_variables,
                            struct context_error *context_error,
                            struct statement_context *out)
{
    if (s == NULL) {
        return 0;
    }

    switch (s->kind) {
        case BINDING_STATEMENT:
        {
            struct type *inferred_type = malloc(sizeof(*inferred_type));
            if (!infer_type(&s->binding_statement.value, scoped_variables, global_context, inferred_type, context_error))
            {
                inferred_type = NULL;
            }
            *out = (struct statement_context) {
                .kind = s->kind,
                .binding_statement = (struct binding_statement_context) {
                    .binding_statement = &s->binding_statement,
                    .inferred_type = inferred_type,
                },
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            return 1;
        }
        case RETURN_STATEMENT:
        {
            struct type *inferred_type = malloc(sizeof(*inferred_type));
            if (!infer_type(&s->expression, scoped_variables, global_context, inferred_type, context_error))
            {
                inferred_type = NULL;
            }
            *out = (struct statement_context) {
                .kind = s->kind,
                .return_statement = (struct return_statement_context) {
                    .e = &s->expression,
                    .inferred_return_type = inferred_type
                },
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables)
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
                                context_error,
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
                        .scoped_variables = copy_scoped_variables(&fn_scoped_variables)
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
                        .scoped_variables = NULL
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
                        context_error,
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
                .scoped_variables = copy_scoped_variables(scoped_variables)
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
                    context_error,
                    success))
            {
                return 0;
            }

            int else_exists = contextualise_statement(
                s->if_statement.else_statement,
                global_context,
                scoped_variables,
                context_error,
                else_statement);
            
            *out = (struct statement_context) {
                .kind = s->kind,
                .if_statement_context = (struct if_statement_context) {
                    .condition = s->if_statement.condition,
                    .success_statement = success,
                    .else_statement = else_exists ? else_statement : NULL
                },
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables)
            };
            return 1;
        }
        case ACTION_STATEMENT:
        {
            struct type *inferred_type = malloc(sizeof(*inferred_type));
            infer_type(&s->expression, scoped_variables, global_context, inferred_type, context_error);
            *out = (struct statement_context) {
                .kind = s->kind,
                .global_context = global_context,
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .action_statement_context = (struct action_statement_context) {
                    .e = &s->expression,
                    .inferred_expression_type = inferred_type
                }
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
                context_error,
                do_statement))
            {
                return 0;
            }

            *out = (struct statement_context) {
                .kind = s->kind,
                .while_loop_statement = (struct while_loop_statement_context) {
                    .condition = s->while_loop_statement.condition,
                    .do_statement = do_statement
                },
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .global_context = global_context
            };
            return 1;
        }
        case BREAK_STATEMENT:
        {
            *out = (struct statement_context) {
                .kind = s->kind,
                .scoped_variables = copy_scoped_variables(scoped_variables),
                .global_context = global_context
            };
            return 1;
        }
        case INCLUDE_STATEMENT:
        {
            *out = (struct statement_context) {
                .kind = s->kind,
                .global_context = global_context,
                .scoped_variables = NULL,
                .include_statement = s->include_statement
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
                  struct context_error *context_error)
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

    if (!generate_global_context(s, &out->global_context, context_error)) return 0;
    struct list_scoped_variable scoped_variables = list_create(scoped_variable, 10);

    for (size_t i = 0; i < s->size; i++) {
        struct statement_context c = {0};
        if (!contextualise_statement(&s->data[i], &out->global_context, &scoped_variables, context_error, &c)) return 0;
        list_append(&out->statements, c);
    }
    
    #ifdef DEBUG_CONTEXT
        for (size_t i = 0; i < out->statements.size; i++) {
            show_statement_context(&out->statements.data[i]);
            printf("\n\n");
        }
    #endif
    return 1;
}

#ifdef DEBUG_CONTEXT
void print_type(struct type *ty) {
    if (ty == NULL) {
        printf("none");
        return;
    }

    switch (ty->kind) {
        case TY_PRIMITIVE:
        {
            switch (ty->primitive_type) {
                case UNIT:
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
            print_type(s->binding_statement.inferred_type);
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
            print_type(s->return_statement.inferred_return_type);
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
