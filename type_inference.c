#include "type_inference.h"
#include "ast.h"
#include "utils.h"
#include <assert.h>
#include <string.h>
#include "utils.h"
#include "context.h"

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

