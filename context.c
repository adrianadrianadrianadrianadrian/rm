#include "ast.h"
#include "utils.h"
#include <assert.h>
#include "utils.h"

struct global_context {
    struct list_type fn_types;
    struct list_type data_types;
};

typedef struct context {
    struct global_context *global_context;
    struct list_key_type_pair scoped_variables;
    struct statement *self;
} context;

struct error {
    struct list_char message;
};

struct_list(context);

int get_scoped_variable_type(struct list_key_type_pair *scoped_variables,
                             struct global_context *c,
					         struct list_char ident_name,
							 struct type *out)
{
    for (size_t i = 0; i < scoped_variables->size; i++) {
        struct key_type_pair pair = scoped_variables->data[i];
        if (list_char_eq(&pair.field_name, &ident_name)) {
            *out = *pair.field_type;
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
                           struct list_char struct_name,
                           struct type *out,
                           struct error *err)
{
    for (size_t i = 0; i < c->data_types.size; i++) {
        struct type this = c->data_types.data[i];
        if (this.kind != TY_STRUCT) {
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

int infer_type(struct expression *e,
               struct list_key_type_pair *scoped_variables,
               struct global_context *c,
               struct type *out,
               struct error *err)
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
                    return find_struct_definition(c, *e->literal.struct_enum.name, out, err);
                }
                case LITERAL_ENUM:
                {
                    *out = (struct type) {
                        .kind = TY_ENUM,
                        .name = e->literal.struct_enum.name,
                        .struct_type = (struct struct_type) {
                            .predefined = 1,
                        }
                    };
                    return 1;
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
            if (e->binary.binary_op == DOT_BINARY)
            {
                struct type l_type = {0};
                if (infer_type(e->binary.l, scoped_variables, c, &l_type, err))
                {
                    if (l_type.kind == TY_STRUCT)
                    {
                        assert(e->binary.r->kind == LITERAL_EXPRESSION);
                        for (size_t i = 0; i < l_type.struct_type.pairs.size; i++) {
                            struct key_type_pair pair = l_type.struct_type.pairs.data[i];
                            if (list_char_eq(&pair.field_name, e->binary.r->literal.name)) {
                                *out = *pair.field_type;
                                return 1;
                            }
                        }
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
            for (size_t i = 0; i < c->fn_types.size; i++) {
                if (list_char_eq(e->function.function_name, c->fn_types.data[i].name)) {
                    *out = *c->fn_types.data[i].function_type.return_type;
                    return 1;
                }
            }

            for (size_t i = 0; i < scoped_variables->size; i++) {
                struct type *fn = scoped_variables->data[i].field_type;
                if (fn->kind == TY_FUNCTION) {
                    if (list_char_eq(e->function.function_name, &scoped_variables->data[i].field_name)) {
                        *out = *fn;
                        return 1;
                    }
                }
            }
            return 0;
        }
    }
}

int contextualize(struct list_statement *s,
                  struct list_context *out,
                  struct error *error)
{
    if (s->size == 0) {
        return 0;
    }

    *out = list_create(context, s->size);
    struct global_context *global_context = malloc(sizeof(*global_context));
    *global_context = (struct global_context) {
        .fn_types = list_create(type, 100),
        .data_types = list_create(type, 100)
    };

    for (size_t i = 0; i < s->size; i++) {
        struct statement *current = &s->data[i];
        switch (current->kind) {
            case TYPE_DECLARATION_STATEMENT:
            {
                switch (current->type_declaration.type.kind) {
                    case TY_STRUCT:
                    case TY_ENUM:
                    {
                        list_append(&global_context->data_types, current->type_declaration.type);
                        break;
                    }
                    case TY_FUNCTION:
                    {
                        struct context fn_context = (struct context) {
                            .global_context = global_context,
                            .scoped_variables = list_create(key_type_pair, 0),
                            .self = current
                        };
                        list_append(out, fn_context);
                        list_append(&global_context->fn_types, current->type_declaration.type);
                        struct list_key_type_pair *scoped_variables = malloc(sizeof(*scoped_variables));
                        *scoped_variables = list_create(key_type_pair, 10);

                        struct function_type fn = current->type_declaration.type.function_type;
                        for (size_t i = 0; i < fn.params.size; i++)
                        {
                            struct key_type_pair param = fn.params.data[i];
                            struct type *complete_type = malloc(sizeof(*complete_type));
                            if (param.field_type->kind == TY_STRUCT) {
                                if (find_struct_definition(global_context,
                                                           *param.field_type->name,
                                                           complete_type,
                                                           error))
                                {
                                    param.field_type = complete_type;
                                }
                            }

                            list_append(scoped_variables, param);
                        } 
                        
                        for (size_t i = 0; i < current->type_declaration.statements->size; i++)
                        {
                            struct statement *self = &current->type_declaration.statements->data[i];
                            if (self->kind == BINDING_STATEMENT) {
                                if (self->binding_statement.has_type) {
                                    struct key_type_pair pair = (struct key_type_pair) {
                                        .field_name = self->binding_statement.variable_name,
                                        .field_type = &self->binding_statement.variable_type
                                    };
                                    list_append(scoped_variables, pair);
                                } else {
                                    struct type *inferred_type = malloc(sizeof(*inferred_type));
                                    if (!infer_type(&self->binding_statement.value,
                                                   scoped_variables,
                                                   global_context,
                                                   inferred_type,
                                                   error))
                                    {
                                        append_list_char_slice(&error->message, "Type must be specified.");
                                        return 0;
                                    }
                                    struct key_type_pair pair = (struct key_type_pair) {
                                        .field_name = self->binding_statement.variable_name,
                                        .field_type = inferred_type
                                    };
                                    list_append(scoped_variables, pair);
                                }
                            }

                            struct key_type_pair *copy = malloc(sizeof(*copy) * scoped_variables->size);
                            memcpy(copy, scoped_variables->data, scoped_variables->size * sizeof(key_type_pair));
                            
                            struct context c = (struct context) {
                                .self = self,
                                .scoped_variables = (struct list_key_type_pair) {
                                    .data = copy,   
                                    .size = scoped_variables->size,
                                    .capacity = scoped_variables->capacity
                                },
                                .global_context = global_context
                            };
                            list_append(out, c);
                        }
                        break;
                    }
                    case TY_PRIMITIVE:
                        break;
                }
                 break;
            }
            case INCLUDE_STATEMENT:
            {
                // includes
                break;
            }
            case BINDING_STATEMENT:
            case IF_STATEMENT:
            case RETURN_STATEMENT:
            case BLOCK_STATEMENT:
            case ACTION_STATEMENT:
            case WHILE_LOOP_STATEMENT:
            case BREAK_STATEMENT:
            case SWITCH_STATEMENT:
            {
                fprintf(stderr, "Not allowed as top level");
                exit(-1);
            }
        }
    }

    return 1;
}

void debug_statement(struct statement *s)
{
    switch (s->kind) {
        case BINDING_STATEMENT:
        {
            printf("statement::binding::%s\n", s->binding_statement.variable_name.data);
            break;
        }
        case TYPE_DECLARATION_STATEMENT:
        {
            printf("statement::type_declaration::%s\n", s->type_declaration.type.name->data);
            break;
        }
        case IF_STATEMENT:
        case RETURN_STATEMENT:
        case BLOCK_STATEMENT:
        case ACTION_STATEMENT:
        case WHILE_LOOP_STATEMENT:
        case BREAK_STATEMENT:
        case INCLUDE_STATEMENT:
        case SWITCH_STATEMENT:
            printf("statement::kind::%d\n", s->kind);
            break;
    }
}

void debug_context(struct context *c) {
    debug_statement(c->self);
    printf("functions [\n");
    for (size_t i = 0; i < c->global_context->fn_types.size; i++) {
        printf("\t%s\n", c->global_context->fn_types.data[i].name->data);
    }
    printf("]\n");
    printf("variables [\n");
    for (size_t i = 0; i < c->scoped_variables.size; i++) {
        printf("\t%s:%d\n", c->scoped_variables.data[i].field_name.data, c->scoped_variables.data[i].field_type->kind);
    }
    printf("]\n");
    printf("\n");
}
