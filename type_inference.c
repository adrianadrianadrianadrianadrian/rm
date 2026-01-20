#include "type_inference.h"
#include "ast.h"
#include "utils.h"
#include <assert.h>

int get_scoped_variable_type(struct list_scoped_variable *scoped_variables,
                             struct global_context *c,
					         struct list_char ident_name,
							 struct type *out)
{
    for (size_t i = 0; i < scoped_variables->size; i++) {
        struct scoped_variable scoped = scoped_variables->data[i];
        if (list_char_eq(&scoped.name, &ident_name)) {
            *out = scoped.type;
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
                           struct list_char *error)
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
    
    append_list_char_slice(error, "struct `");
    append_list_char_slice(error, struct_name->data);
    append_list_char_slice(error, "` does not exist.");
    return 0;
}

int find_enum_definition(struct global_context *c,
                         struct list_char *enum_name,
                         struct type *out,
                         struct list_char *error)
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
    
    append_list_char_slice(error, "enum `");
    append_list_char_slice(error, enum_name->data);
    append_list_char_slice(error, "` does not exist.");
    return 0;
}

int get_field_type(struct list_key_type_pair *pairs,
                   struct list_char *field_name,
                   struct global_context *global_context,
                   struct type *out,
                   struct list_char *error)
{
    for (size_t i = 0; i < pairs->size; i++) {
        if (list_char_eq(field_name, &pairs->data[i].field_name)) {
            struct type *found = pairs->data[i].field_type;

            if (found->kind == TY_STRUCT && found->struct_type.predefined) {
                if (!find_struct_definition(global_context, found->name, found, error)) return 0;
            } else if (found->kind == TY_ENUM && found->enum_type.predefined) {
                if (!find_enum_definition(global_context, found->name, found, error)) return 0;
            }
            
            *out = *found;
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
            return find_struct_definition(global_context, e->struct_enum.name, out, error);
        case LITERAL_ENUM:
            return find_enum_definition(global_context, e->struct_enum.name, out, error);
        case LITERAL_NAME:
        {
            for (size_t i = 0; i < scoped_variables->size; i++) {
                if (list_char_eq(e->name, &scoped_variables->data[i].name)) {
                    struct type *t = &scoped_variables->data[i].type;
                    // TODO: enums
                    if (t->kind == TY_STRUCT) {
                        if (!find_struct_definition(global_context, t->name, t, error)) return 0;
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

            append_list_char_slice(error, "cannot find literal name `");
            append_list_char_slice(error, e->name->data);
            append_list_char_slice(error, "`.");
            return 0;
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
        case LITERAL_NULL:
        {
            struct list_type_modifier modifiers = list_create(type_modifier, 1);
            struct type_modifier array = (struct type_modifier) {
                .kind = NULLABLE_MODIFIER_KIND
            };
            list_append(&modifiers, array);
            *out = (struct type) {
                .kind = TY_ANY,
                .modifiers = modifiers
            };
            return 1;
        }
        case LITERAL_HOLE:
            return 1;
    }

    UNREACHABLE("infer_literal_expression_type: dropped out of switch.");
}

int infer_function_type(struct type *matched_fn,
                        struct global_context *global_context,
                        size_t value_count,
                        struct type *out,
                        struct list_char *error_message)
{
    assert(matched_fn->kind == TY_FUNCTION);
    if (matched_fn->function_type.params.size < value_count) {
        append_list_char_slice(error_message, "too many values provided to `");
        append_list_char_slice(error_message, matched_fn->name->data);
        append_list_char_slice(error_message, "`");
        return 0;
    }
    
    if (matched_fn->function_type.params.size == value_count) {
        if (matched_fn->function_type.return_type->kind == TY_STRUCT) {
            return find_struct_definition(global_context,
                                          matched_fn->function_type.return_type->name,
                                          out,
                                          error_message);
        }

        *out = *matched_fn->function_type.return_type;
        return 1;
    }
    
    struct type *inferred = malloc(sizeof(*inferred));
    struct list_key_type_pair params = list_create(key_type_pair, matched_fn->function_type.params.size - value_count);
    for (size_t i = value_count; i < matched_fn->function_type.params.size; i++) {
        list_append(&params, matched_fn->function_type.params.data[i]);
    }

    *out = (struct type) {
        .kind = TY_FUNCTION,
        .function_type = (struct function_type) {
            .params = params,
            .return_type = matched_fn->function_type.return_type
        }
    };
    return 1;
}


int infer_expression_type(struct expression *e,
                          struct global_context *global_context,
                          struct context *context,
                          struct list_scoped_variable *scoped_variables,
                          struct type *out,
                          struct list_char *error)
{
    switch (e->kind) {
        case LITERAL_EXPRESSION:
        {
            if (!infer_literal_expression_type(&e->literal,
                                               global_context,
                                               scoped_variables,
                                               out,
                                               error))
            {
                return 0;
            }
            lut_add(&context->expression_type_lookup, e->id, *out);
            return 1;
        }
        case UNARY_EXPRESSION:
        {
            if (!infer_expression_type(e->unary.expression,
                                       global_context,
                                       context,
                                       scoped_variables,
                                       out,
                                       error))
            {
                return 0;
            }
            lut_add(&context->expression_type_lookup, e->id, *out);
            return 1;
        }
        case BINARY_EXPRESSION:
        {
            struct type left = {0};
            if (!infer_expression_type(e->binary.l,
                                       global_context,
                                       context,
                                       scoped_variables,
                                       &left,
                                       error))
            {
                return 0;
            }

            struct type right = {0};
            if (!infer_expression_type(e->binary.r,
                                       global_context,
                                       context,
                                       scoped_variables,
                                       &right,
                                       error))
            {
                return 0;
            }
            
            // TODO
            switch (e->binary.binary_op) {
                case PLUS_BINARY:
                case MINUS_BINARY:
                case MULTIPLY_BINARY:
                case ASSIGN_BINARY:
                case BITWISE_OR_BINARY:
                case BITWISE_AND_BINARY:
                {
                    TODO("binary ops");
                    return 0;
                }
                case GREATER_THAN_BINARY:
                case LESS_THAN_BINARY:
                case EQUAL_TO_BINARY:
                case OR_BINARY:
                case AND_BINARY:
                {
                    *out = (struct type) {
                        .kind = TY_PRIMITIVE,
                        .primitive_type = BOOL
                    };
                    lut_add(&context->expression_type_lookup, e->id, *out);
                    return 1;
                }
            }
            return 1;
        }
        case GROUP_EXPRESSION:
        {
            if (!infer_expression_type(e->grouped,
                                       global_context,
                                       context,
                                       scoped_variables,
                                       out,
                                       error))
            {
                return 0;
            }
            lut_add(&context->expression_type_lookup, e->id, *out);
            return 1;
        }
        case FUNCTION_EXPRESSION:
        {
            size_t value_count = e->function.params->size;
            
            for (size_t i = 0; i < global_context->fn_types.size; i++) {
                struct type *global_fn = &global_context->fn_types.data[i];
                if (list_char_eq(e->function.function_name, global_context->fn_types.data[i].name)) {
                    return infer_function_type(global_fn, global_context, value_count, out, error);
                }
            }

            for (size_t i = 0; i < scoped_variables->size; i++) {
                struct type *fn = &scoped_variables->data[i].type;
                if (fn->kind == TY_FUNCTION) {
                    if (list_char_eq(e->function.function_name, &scoped_variables->data[i].name)) {
                        return infer_function_type(fn, global_context, value_count, out, error);
                    }
                }
            }

            append_list_char_slice(error, "the function `");
            append_list_char_slice(error, e->function.function_name->data);
            append_list_char_slice(error, "` does not exist.");
            return 0;
        }
        case MEMBER_ACCESS_EXPRESSION:
        {
            struct type accessed = {0};
            if (!infer_expression_type(e->member_access.accessed,
                                       global_context,
                                       context,
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
                                out,
                                error))
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

int infer_full_type(struct type *incomplete_type,
                    struct global_context *global_context,
                    struct list_scoped_variable *scoped_variables,
                    struct type *out,
                    struct list_char *error)
{
    switch (incomplete_type->kind) {
        case TY_FUNCTION:
        {
            TODO("infer full type for functions. e.g a_param: fn read_all");
        }
        case TY_STRUCT:
        {
            return find_struct_definition(global_context, incomplete_type->name, out, error);
        }
        case TY_ENUM:
        {
            return find_enum_definition(global_context, incomplete_type->name, out, error);
        }
        case TY_ANY:
        case TY_PRIMITIVE:
        {
            *out = *incomplete_type;
            return 1;
        }
    }
    
    UNREACHABLE("dropped out of type switch within infer_full_type");
}
