#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "ast.h"
#include "lexer.h"
#include "error.h"
#include "parser.h"

#define parser_t int (*)(struct token_buffer *, void *, struct error *error)
int try_parse(struct token_buffer *s,
              void *out,
              struct error *error,
              int (*parser)(struct token_buffer *, void *, struct error *error))
{
    if (error->errored) {
        return 0;
    }

    size_t start = s->current_position;
    if (!parser(s, out, error)) {
        seek_back_token(s, s->current_position - start);
        return 0;
    }
    
    return 1;
}

void add_error_inner(struct token_buffer *s, struct error *out, char *message)
{
    size_t position = s->current_position;
    if (s->current_position > 1) {
        position -= 1;
    }

    struct token_metadata *tok_metadata = get_token_metadata(s, position);
    add_error(tok_metadata->row, tok_metadata->col, tok_metadata->file_name, out, message);
}

struct statement_metadata statement_metadata(const struct token_buffer *s)
{
    struct token_metadata *tok_metadata = get_token_metadata(s, s->current_position);
    return (struct statement_metadata) {
        .file_name = tok_metadata->file_name,
        .row = tok_metadata->row,
        .col = tok_metadata->col
    };
}

int is_primitive(struct list_char *raw, enum primitive_type *out)
{
    if (strcmp(raw->data, "void") == 0) {
        *out = VOID;
        return 1;
    }

    if (strcmp(raw->data, "bool") == 0) {
        *out = BOOL;
        return 1;
    }

    if (strcmp(raw->data, "i8") == 0) {
        *out = I8;
        return 1;
    }

    if (strcmp(raw->data, "u8") == 0) {
        *out = U8;
        return 1;
    }

    if (strcmp(raw->data, "i16") == 0) {
        *out = I16;
        return 1;
    }

    if (strcmp(raw->data, "u16") == 0) {
        *out = U16;
        return 1;
    }

    if (strcmp(raw->data, "i32") == 0) {
        *out = I32;
        return 1;
    }

    if (strcmp(raw->data, "u32") == 0) {
        *out = U32;
        return 1;
    }

    if (strcmp(raw->data, "i64") == 0) {
        *out = I64;
        return 1;
    }

    if (strcmp(raw->data, "u64") == 0) {
        *out = U64;
        return 1;
    }

    if (strcmp(raw->data, "usize") == 0) {
        *out = USIZE;
        return 1;
    }

    if (strcmp(raw->data, "f32") == 0) {
        *out = F32;
        return 1;
    }

    if (strcmp(raw->data, "f64") == 0) {
        *out = F64;
        return 1;
    }

    if (strcmp(raw->data, "IO") == 0) {
        *out = IO;
        return 1;
    }

    return 0;
}

struct key_type_pair create_key_type_pair() {
    return (struct key_type_pair) {
        .field_name = list_create(char, 10),
        .field_type = malloc(sizeof(struct type))
    };
}

int parse_pointer_type_modifier(struct token_buffer *tb,
                                struct type_modifier *out,
                                struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, STAR)) return 0;
    *out = (struct type_modifier) {
        .kind = POINTER_MODIFIER_KIND,
    };

    return 1;
}

int parse_nullable_type_modifier(struct token_buffer *tb,
                                 struct type_modifier *out,
                                 struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, QUESTION_MARK)) return 0;
    *out = (struct type_modifier) {
        .kind = NULLABLE_MODIFIER_KIND,
    };

    return 1;
}

int parse_array_type_modifier(struct token_buffer *tb,
                              struct type_modifier *out,
                              struct error *error)
{
    struct token tmp = {0};
    int literal_size = 0;
    int literally_sized = 0;
    int reference_sized = 0;
    struct list_char *reference_name = NULL;
    if (!get_token_type(tb, &tmp, OPEN_SQUARE_PAREN))  return 0;
    if (get_token_type(tb, &tmp, NUMERIC)) {
        literally_sized = 1;
        literal_size = (int)tmp.numeric; // TODO: numeric token needs improving
    } else if (get_token_type(tb, &tmp, IDENTIFIER)) {
        reference_sized = 1;
        reference_name = tmp.identifier;
    }
    if (!get_token_type(tb, &tmp, CLOSE_SQUARE_PAREN)) return 0;
    *out = (struct type_modifier) {
        .kind = ARRAY_MODIFIER_KIND,
        .array_modifier = (struct array_type_modifier) {
            .literal_size = literal_size,
            .literally_sized = literally_sized,
            .reference_sized = reference_sized,
            .reference_name = reference_name
        }
    };

    return 1;
}

int parse_mutable_type_modifier(struct token_buffer *tb,
                                struct type_modifier *out,
                                struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, MUTABLE_KEYWORD)) return 0;
    *out = (struct type_modifier) {
        .kind = MUTABLE_MODIFIER_KIND,
    };

    return 1;
}

int parse_type_modifier(struct token_buffer *tb, struct type_modifier *out, struct error *error)
{
    return try_parse(tb, out, error, (parser_t)parse_pointer_type_modifier)
        || try_parse(tb, out, error, (parser_t)parse_nullable_type_modifier)
        || try_parse(tb, out, error, (parser_t)parse_array_type_modifier)
        || try_parse(tb, out, error, (parser_t)parse_mutable_type_modifier);
}

struct list_type_modifier parse_modifiers(struct token_buffer *tb, struct error *error)
{
    struct list_type_modifier modifiers = list_create(type_modifier, 5);
    struct type_modifier modifier = {0};
    while (parse_type_modifier(tb, &modifier, error)) {
        list_append(&modifiers, modifier);
    }
    return modifiers;
}

int parse_type(struct token_buffer *s,
               struct type *out,
               int named_fn,
               int predefined,
               struct error *error);

int parse_key_type_pairs(struct token_buffer *s,
                         struct list_key_type_pair *out,
                         struct error *error)
{
    struct token tmp = {0};
    int should_continue = 1;

    while (should_continue) {
        struct key_type_pair pair = create_key_type_pair();
        if (!get_token_type(s, &tmp, IDENTIFIER)) return 0;
        pair.field_name = *tmp.identifier;
        if (!get_token_type(s, &tmp, COLON)) return 0;
        if (!parse_type(s, pair.field_type, 0, 1, error)) return 0;
        list_append(out, pair);
        should_continue = get_token_type(s, &tmp, COMMA);
    }

    return 1;
}

int parse_function_type(struct token_buffer *s,
                        struct type *out,
                        int named,
                        struct error *error)
{
    struct token tmp;
    struct token name;
    struct list_key_type_pair params = list_create(key_type_pair, 10);
    struct type *return_type = malloc(sizeof(*return_type));
    *return_type = (struct type) {
        .kind = TY_PRIMITIVE,
        .primitive_type = VOID
    };

    if (named && !get_token_type(s, &name, IDENTIFIER)) {
        add_error_inner(s, error, "a function must have a name in this position.");
        return 0;
    }

    if (!get_token_type(s, &tmp, OPEN_ROUND_PAREN)) {
        add_error_inner(s, error, "missing `(`.");
        return 0;
    }

    parse_key_type_pairs(s, &params, error);
    if (!get_token_type(s, &tmp, CLOSE_ROUND_PAREN)) {
        add_error_inner(s, error, "missing `)`.");
        return 0;
    }

    if (get_token_type(s, &tmp, RIGHT_ARROW)) {
        if (!parse_type(s, return_type, 0, 1, error)) {
            add_error_inner(s, error, "unknown return type.");
            return 0;
        }
    }

    *out = (struct type) {
        .kind = TY_FUNCTION,
        .function_type = (struct function_type) {
            .params = params,
            .return_type = return_type
        },
        .name = name.identifier,
        .modifiers = out->modifiers
    };

    return 1;
}

int parse_struct_type(struct token_buffer *s,
                      struct type *out,
                      int predefined_type,
                      struct error *error)
{
    struct token name = {0};
    struct token tmp = {0};
    struct list_key_type_pair pairs = list_create(key_type_pair, 10);

    if (!get_token_type(s, &name, IDENTIFIER)) {
        add_error_inner(s, error, "a struct must have a name.");
        return 0;
    }

    if (!predefined_type) {
        if (!get_token_type(s, &tmp, OPEN_CURLY_PAREN)) {
            add_error_inner(s, error, "a struct definition must have a body.");
            return 0;
        }

        if (!parse_key_type_pairs(s, &pairs, error) && error->errored) return 0;
        if (!get_token_type(s, &tmp, CLOSE_CURLY_PAREN)) {
            add_error_inner(s, error, "missing a `}`.");
            return 0;
        }
    }

    *out = (struct type) {
        .kind = TY_STRUCT,
        .name = name.identifier,
        .struct_type = (struct struct_type) {
            .pairs = pairs,
            .predefined = predefined_type
        },
        .modifiers = out->modifiers
    };

    return 1;
}

int parse_enum_type(struct token_buffer *s,
                    struct type *out,
                    int predefined_type,
                    struct error *error)
{
    struct token name = {0};
    struct token tmp = {0};
    struct list_key_type_pair pairs = list_create(key_type_pair, 10);

    if (!get_token_type(s, &name, IDENTIFIER)) {
        add_error_inner(s, error, "a enum must have a name.");
        return 0;
    }

    if (!predefined_type) {
        if (!get_token_type(s, &tmp, OPEN_CURLY_PAREN)) {
            add_error_inner(s, error, "a enum definition must have a body.");
            return 0;
        }

        if (!parse_key_type_pairs(s, &pairs, error) && error->errored) return 0;
        if (!get_token_type(s, &tmp, CLOSE_CURLY_PAREN)) {
            add_error_inner(s, error, "missing a `}`.");
            return 0;
        }
    }

    *out = (struct type) {
        .kind = TY_ENUM,
        .name = name.identifier,
        .enum_type = (struct enum_type) {
            .pairs = pairs,
            .predefined = predefined_type
        },
        .modifiers = out->modifiers
    };

    return 1;
}

int parse_primitive_type(struct token_buffer *s, struct type *out, struct error *error)
{
    struct token tmp = {0};
    enum primitive_type primitive_type;
    struct token name = {0};

    if (!get_token_type(s, &tmp, IDENTIFIER))           return 0;
    if (!is_primitive(tmp.identifier, &primitive_type)) return 0;

    *out = (struct type) {
        .kind = TY_PRIMITIVE,
        .primitive_type = primitive_type,
        .name = name.identifier,
        .modifiers = out->modifiers
    };

    return 1;
}

int parse_type(struct token_buffer *s,
               struct type *out,
               int named_fn,
               int predefined,
               struct error *error)
{
    struct token tmp = {0};
    out->modifiers = parse_modifiers(s, error);

    if (get_token(s, &tmp)) {
        switch (tmp.token_type) {
            case FN_KEYWORD:
                return parse_function_type(s, out, named_fn, error);
            case ENUM_KEYWORD:
                return parse_enum_type(s, out, predefined, error);
            case STRUCT_KEYWORD:
                return parse_struct_type(s, out, predefined, error);
            default:
                seek_back_token(s, 1);
        }
    }

    return try_parse(s, out, error, (parser_t)parse_primitive_type);
}

int parse_expression(struct token_buffer *s, struct expression *out, struct error *error);

int parse_boolean_literal_expression(struct token_buffer *s,
                                     struct literal_expression *out,
                                     struct error *error)
{
    struct token tmp = {0};
    if (get_token_type(s, &tmp, BOOLEAN_TRUE_KEYWORD)) {
        *out = (struct literal_expression) {
            .kind = LITERAL_BOOLEAN,
            .boolean = 1
        };
        return 1;
    }

    if (get_token_type(s, &tmp, BOOLEAN_FALSE_KEYWORD)) {
        *out = (struct literal_expression) {
            .kind = LITERAL_BOOLEAN,
            .boolean = 0
        };
        return 1;
    }

    return 0;
}

int parse_char_literal_expression(struct token_buffer *s,
                                  struct literal_expression *out,
                                  struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(s, &tmp, CHAR_LITERAL)) return 0;

    *out = (struct literal_expression) {
        .kind = LITERAL_CHAR,
        .character = tmp.identifier->data[0]
    };

    return 1;
}

int parse_numeric_literal_expression(struct token_buffer *s,
                                     struct literal_expression *out,
                                     struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(s, &tmp, NUMERIC)) return 0;

    *out = (struct literal_expression) {
        .kind = LITERAL_NUMERIC,
        .numeric = tmp.numeric
    };

    return 1;
}

int parse_str_literal_expression(struct token_buffer *s,
                                 struct literal_expression *out,
                                 struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(s, &tmp, STR_LITERAL)) return 0;

    *out = (struct literal_expression) {
        .kind = LITERAL_STR,
        .str = tmp.identifier
    };

    return 1;
}

int parse_identifier_literal_expression(struct token_buffer *s,
                                        struct literal_expression *out,
                                        struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(s, &tmp, IDENTIFIER)) return 0;

    if (strcmp(tmp.identifier->data, "_") == 0) {
        *out = (struct literal_expression) {
            .kind = LITERAL_HOLE,
        };
        return 1;
    }

    *out = (struct literal_expression) {
        .kind = LITERAL_NAME,
        .name = tmp.identifier
    };

    return 1;
}

int parse_struct_enum_literal_expression(struct token_buffer *s,
                                         struct literal_expression *out,
                                         struct error *error)
{
    struct token tmp = {0};
    struct token name = {0};
    enum literal_expression_kind kind = 0;

    if (get_token_type(s, &tmp, STRUCT_KEYWORD)) {
        kind = LITERAL_STRUCT;
    } else if (get_token_type(s, &tmp, ENUM_KEYWORD)) {
        kind = LITERAL_ENUM;
    } else {
        return 0;
    }

    if (!get_token_type(s, &name, IDENTIFIER))      return 0;
    if (!get_token_type(s, &tmp, OPEN_CURLY_PAREN)) return 0;

    struct list_key_expression pairs = list_create(key_expression, 10);
    int should_continue = 1;
    while (should_continue) {
        struct key_expression pair = {0};
        if (!get_token_type(s, &tmp, IDENTIFIER)) return 0;
        pair.key = tmp.identifier;
        if (!get_token_type(s, &tmp, EQ)) return 0;
        struct expression *e = malloc(sizeof(*e));
        if (!parse_expression(s, e, error)) return 0;
        pair.expression = e;
        list_append(&pairs, pair);
        should_continue = get_token_type(s, &tmp, COMMA);
    }
    
    if (!get_token_type(s, &tmp, CLOSE_CURLY_PAREN)) return 0;

    *out = (struct literal_expression) {
        .kind = kind,
        .struct_enum = (struct literal_struct_enum) {
            .name = name.identifier,
            .key_expr_pairs = pairs
        }
    };

    return 1;
}

int parse_null_literal_expression(struct token_buffer *s,
                                  struct literal_expression *out,
                                  struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(s, &tmp, NULL_KEYWORD)) return 0;

    *out = (struct literal_expression) {
        .kind = LITERAL_NULL,
    };

    return 1;
}

int parse_literal_expression(struct token_buffer *s,
                             struct literal_expression *out,
                             struct error *error)
{
    return try_parse(s, out, error, (parser_t)parse_char_literal_expression)
        || try_parse(s, out, error, (parser_t)parse_str_literal_expression)
        || try_parse(s, out, error, (parser_t)parse_numeric_literal_expression)
        || try_parse(s, out, error, (parser_t)parse_boolean_literal_expression)
        || try_parse(s, out, error, (parser_t)parse_identifier_literal_expression)
        || try_parse(s, out, error, (parser_t)parse_struct_enum_literal_expression)
        || try_parse(s, out, error, (parser_t)parse_null_literal_expression);
}

int parse_unary_operator(struct token_buffer *s,
                         enum unary_operator *out)
{
    struct token tmp = {0};
    if (get_token_type(s, &tmp, BANG)) {
        *out = BANG_UNARY;
        return 1;
    }

    if (get_token_type(s, &tmp, STAR)) {
        *out = STAR_UNARY;
        return 1;
    }

    if (get_token_type(s, &tmp, MINUS)) {
        *out = MINUS_UNARY;
        return 1;
    }

    return 0;
}

int parse_binary_operator(struct token_buffer *s, enum binary_operator *out, struct error *error)
{
    struct token tmp = {0};
    if (get_token_type(s, &tmp, PLUS)) {
        *out = PLUS_BINARY;
        return 1;
    }

    if (get_token_type(s, &tmp, MINUS)) {
        *out = MINUS_BINARY;
        return 1;
    }

    if (get_token_type(s, &tmp, STAR)) {
        *out = MULTIPLY_BINARY;
        return 1;
    }

    if (get_token_type(s, &tmp, PIPE)) {
        if (get_token_type(s, &tmp, PIPE)) {
            *out = OR_BINARY;
        } else {
            *out = BITWISE_OR_BINARY;
        }
        return 1;
    }

    if (get_token_type(s, &tmp, AND)) {
        if (get_token_type(s, &tmp, AND)) {
            *out = AND_BINARY;
        } else {
            *out = BITWISE_AND_BINARY;
        }
        return 1;
    }

    if (get_token_type(s, &tmp, RIGHT_ARROW)) {
        *out = GREATER_THAN_BINARY;
        return 1;
    }

    if (get_token_type(s, &tmp, LEFT_ARROW)) {
        *out = LESS_THAN_BINARY;
        return 1;
    }

    if (get_token_type(s, &tmp, EQ)) {
        if (get_token_type(s, &tmp, EQ)) {
            *out = EQUAL_TO_BINARY;
            return 1;
        } else {
            seek_back_token(s, 1);
            return 0;
        }
    }

    return 0;
}

int parse_function_expression(struct token_buffer *s,
                              struct function_expression *out,
                              struct error *error)
{
    struct token tmp = {0};
    struct token name = {0};
    if (!get_token_type(s, &name, IDENTIFIER))      return 0;
    if (!get_token_type(s, &tmp, OPEN_ROUND_PAREN)) return 0;

    struct list_expression *params = malloc(sizeof(*params));
    *params = list_create(expression, 10);
    int should_continue = 1;

    while (should_continue) {
        struct expression expr = {0};
        if (parse_expression(s, &expr, error)) {
            list_append(params, expr);
        }
        should_continue = get_token_type(s, &tmp, COMMA);
    }

    if (!get_token_type(s, &tmp, CLOSE_ROUND_PAREN)) {
        add_error_inner(s, error, "expected a `)`.");
        return 0;
    }

    *out = (struct function_expression) {
        .function_name = name.identifier,
        .params = params
    };

    return 1;
}

int parse_expression_inner(struct token_buffer *s, struct expression *out, struct error *error) {
    struct token tmp = {0};
    enum unary_operator unary_op;

    if (parse_unary_operator(s, &unary_op)) {
        struct expression *nested = malloc(sizeof(*nested));
        if (!parse_expression_inner(s, nested, error)) return 0;

        *out = (struct expression) {
            .kind = UNARY_EXPRESSION,
            .unary = (struct unary_expression) {
                .unary_operator = unary_op,
                .expression = nested
            }
        };
        return 1;
    }

    if (get_token_type(s, &tmp, OPEN_ROUND_PAREN)) {
        struct expression *nested = malloc(sizeof(*nested));
        if (!parse_expression(s, nested, error)) return 0;
        if (!get_token_type(s, &tmp, CLOSE_ROUND_PAREN)) return 0;
        *out = (struct expression) {
            .kind = GROUP_EXPRESSION,
            .grouped = nested
        };
        return 1;
    }

    struct function_expression function = {0};
    if (try_parse(s, &function, error, (parser_t)parse_function_expression)) {
        *out = (struct expression) {
            .kind = FUNCTION_EXPRESSION,
            .function = function
        };
        return 1;
    }

    struct literal_expression literal = {0};
    if (parse_literal_expression(s, &literal, error)) {
        *out = (struct expression) {
            .kind = LITERAL_EXPRESSION,
            .literal = literal
        };
        return 1;
    }

    return 0;
}

int parse_member_access_expression(struct token_buffer *b,
                                   struct expression *l,
                                   struct expression *out,
                                   struct error *error)
{
    struct token tmp = {0};
    int succeeded = 0;
    while (get_token_type(b, &tmp, DOT) && get_token_type(b, &tmp, IDENTIFIER))
    {
        succeeded = 1;
        struct expression *g = malloc(sizeof(*g));
        struct expression *cpy_l = malloc(sizeof(*cpy_l));
        *cpy_l = *l;
        *g = (struct expression) {
            .kind = MEMBER_ACCESS_EXPRESSION,
            .member_access = (struct member_access_expression) {
                .accessed = cpy_l,
                .member_name = tmp.identifier
            }
        };

        *l = (struct expression) {
            .kind = GROUP_EXPRESSION,
            .grouped = g
        };
    }
    
    if (succeeded) {
        *out = *l;
    }

    return succeeded;
}

int parse_expression(struct token_buffer *s, struct expression *out, struct error *error) {
    enum binary_operator op;
    int parsed_left = 0;
    struct expression *l = malloc(sizeof(*l));
    struct expression *r = malloc(sizeof(*r));

    if (parse_expression_inner(s, l, error)) {
        parsed_left = 1;
    } else {
        return 0;
    }

    if (peek_token_type(s, DOT) && !parse_member_access_expression(s, l, out, error)) return 0;
    if (parse_binary_operator(s, &op, error) && parse_expression(s, r, error)) {
        *out = (struct expression) {
            .kind = BINARY_EXPRESSION,
            .binary = (struct binary_expression) {
                .binary_op = op,
                .l = l,
                .r = r
            }
        };
        return 1;
    }

    if (parsed_left) {
        *out = *l;
        return 1;
    }

    return 0;
}

int parse_switch_pattern(struct token_buffer *tb, struct switch_pattern *out, struct error *error);

int parse_variable_or_underscore_pattern(struct token_buffer *tb,
                                         struct switch_pattern *out,
                                         struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, IDENTIFIER)) return 0;
    if (strcmp(tmp.identifier->data, "_") == 0) {
        *out = (struct switch_pattern) {
            .switch_pattern_kind = UNDERSCORE_PATTERN_KIND,
            .variable_pattern = (struct variable_pattern) {
                .variable_name = *tmp.identifier
            }
        };
    } else {
        *out = (struct switch_pattern) {
            .switch_pattern_kind = VARIABLE_PATTERN_KIND,
            .variable_pattern = (struct variable_pattern) {
                .variable_name = *tmp.identifier
            }
        };
    }

    return 1;
}

int parse_string_pattern(struct token_buffer *tb, struct switch_pattern *out, struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, STR_LITERAL)) return 0;
    *out = (struct switch_pattern) {
        .switch_pattern_kind = STRING_PATTERN_KIND,
        .string_pattern = (struct string_pattern) {
            .str = *tmp.identifier
        }
    };

    return 1;
}

int parse_number_pattern(struct token_buffer *tb, struct switch_pattern *out, struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, NUMERIC)) return 0;
    *out = (struct switch_pattern) {
        .switch_pattern_kind = NUMBER_PATTERN_KIND,
        .number_pattern = (struct number_pattern) {
            .number = tmp.numeric
        }
    };
    
    return 1;
}

int parse_rest_pattern(struct token_buffer *tb, struct switch_pattern *out, struct error *error)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, DOT)) return 0;
    if (!get_token_type(tb, &tmp, DOT)) return 0;

    *out = (struct switch_pattern) {
        .switch_pattern_kind = REST_PATTERN_KIND
    };
    
    return 1;
}

int parse_array_pattern(struct token_buffer *tb, struct switch_pattern *out, struct error *error)
{
    struct token tmp = {0};
    struct list_switch_pattern *patterns = malloc(sizeof(*patterns));
    *patterns = list_create(switch_pattern, 10);
    int should_continue = 1;

    if (!get_token_type(tb, &tmp, OPEN_SQUARE_PAREN)) return 0;
    while (should_continue) {
        struct switch_pattern p = {0};
        if (parse_switch_pattern(tb, &p, error)) {
            list_append(patterns, p);
        }
        should_continue = get_token_type(tb, &tmp, COMMA);
    }
    if (!get_token_type(tb, &tmp, CLOSE_SQUARE_PAREN)) return 0;
    *out = (switch_pattern) {
        .switch_pattern_kind = ARRAY_PATTERN_KIND,
        .array_pattern = (struct array_pattern) {
            .patterns = patterns
        }
    };

    return 1;
}

int parse_key_pattern_pair(struct token_buffer *tb, struct key_pattern_pair *out, struct error *error)
{
    struct token tmp = {0};
    struct list_char key = {0};
    struct switch_pattern *pattern = malloc(sizeof(*pattern));
    if (!get_token_type(tb, &tmp, IDENTIFIER)) {
        if (!parse_rest_pattern(tb, pattern, error)) return 0;
        *out = (struct key_pattern_pair) {
            .key = key,
            .pattern = pattern
        };
        return 1;
    }
    key = *tmp.identifier;
    if (!get_token_type(tb, &tmp, COLON)) return 0;
    if (parse_switch_pattern(tb, pattern, error)) return 0;
    *out = (struct key_pattern_pair) {
        .key = key,
        .pattern = pattern
    };

    return 0;
}

int parse_object_pattern(struct token_buffer *tb, struct switch_pattern *out, struct error *error)
{
    struct token tmp = {0};
    int should_continue = 1;
    struct list_key_pattern_pair pairs = list_create(key_pattern_pair, 10);

    if (!get_token_type(tb, &tmp, OPEN_CURLY_PAREN)) return 0;
    while (should_continue) {
        struct key_pattern_pair p = {0};
        if (parse_key_pattern_pair(tb, &p, error)) {
            list_append(&pairs, p);
        }
        should_continue = get_token_type(tb, &tmp, COMMA);
    }
    if (!get_token_type(tb, &tmp, CLOSE_CURLY_PAREN)) return 0;
    *out = (struct switch_pattern) {
        .switch_pattern_kind = OBJECT_PATTERN_KIND,
        .object_pattern = (struct object_pattern) {
            .pairs = pairs
        }
    };

    return 1;
}

int parse_switch_pattern(struct token_buffer *tb, struct switch_pattern *out, struct error *error)
{
    return try_parse(tb, out, error, (parser_t)parse_object_pattern)
        || try_parse(tb, out, error, (parser_t)parse_array_pattern)
        || try_parse(tb, out, error, (parser_t)parse_rest_pattern)
        || try_parse(tb, out, error, (parser_t)parse_number_pattern)
        || try_parse(tb, out, error, (parser_t)parse_string_pattern)
        || try_parse(tb, out, error, (parser_t)parse_variable_or_underscore_pattern);
}

int parse_statement(struct token_buffer *s, struct statement *out, struct error *error);

int parse_break_statement(struct token_buffer *s, struct statement *out, struct error *error)
{
    struct statement_metadata metadata = statement_metadata(s);
    struct token tmp = {0};
    if (!get_token_type(s, &tmp, BREAK_KEYWORD)) return 0;
    if (!get_token_type(s, &tmp, SEMICOLON)) {
        add_error_inner(s, error, "a break statement must end with a semicolon.");
        return 0;
    }

    *out = (struct statement) {
        .kind = BREAK_STATEMENT,
        .metadata = metadata
    };

    return 1;
}

int parse_return_statement(struct token_buffer *s,
                           struct statement *out,
                           struct error *error)
{
    struct statement_metadata metadata = statement_metadata(s);
    struct token tmp = {0};
    struct expression expression = {0};

    if (!get_token_type(s, &tmp, RETURN_KEYWORD)) return 0;
    if (!parse_expression(s, &expression, error)) {
        if (get_token_type(s, &tmp, SEMICOLON)) {
            *out = (struct statement) {
                .kind = RETURN_STATEMENT,
                .expression = (struct expression) {
                    .kind = VOID_EXPRESSION
                },
                .metadata = metadata
            };
            return 1;
        }

        add_error_inner(s, error, "a return statement must return a valid expression.");
        return 0;
    }

    if (!get_token_type(s, &tmp, SEMICOLON)) {
        add_error_inner(s, error, "a statement must end with a semicolon.");
        return 0;
    }

    *out = (struct statement) {
        .kind = RETURN_STATEMENT,
        .expression = expression,
        .metadata = metadata
    };
    return 1;
}

int parse_binding_statement(struct token_buffer *s,
                            struct statement *out,
                            struct error *error)
{
    struct statement_metadata metadata = statement_metadata(s);
    struct token tmp = {0};
    struct type type = {0};
    struct expression expression = {0};
    struct list_char variable_name = {0};
    int has_type = 0;
    
    if (!get_token_type(s, &tmp, LET_KEYWORD)) return 0;
    if (!get_token_type(s, &tmp, IDENTIFIER)) {
        add_error_inner(s, error, "a let binding must have a name.");
        return 0;
    }
    variable_name = *tmp.identifier;

    if (get_token_type(s, &tmp, COLON)) {
        if (!parse_type(s, &type, 0, 1, error)) {
            if (get_token_type(s, &tmp, IDENTIFIER)) {
                struct list_char error_message = list_create(char, 30);
                append_list_char_slice(&error_message, "unknown type");
                append_list_char_slice(&error_message, " `");
                append_list_char_slice(&error_message, tmp.identifier->data);
                append_list_char_slice(&error_message, "`.");
                add_error_inner(s, error, error_message.data);
                return 0;
            }

            add_error_inner(s, error, "a type annotation is required after a `:` in a binding statement.");
            return 0;
        }
        has_type = 1;
    }

    if (!get_token_type(s, &tmp, EQ)) {
        add_error_inner(s, error, "expected a `=`.");
        return 0;
    }

    if (!parse_expression(s, &expression, error)) {
        struct list_char msg = list_create(char, 50);
        append_list_char_slice(&msg, "the variable `");
        append_list_char_slice(&msg, variable_name.data);
        append_list_char_slice(&msg, "` must be bound to a valid expression.");
        add_error_inner(s, error, msg.data);
        return 0; 
    }

    if (!get_token_type(s, &tmp, SEMICOLON)) {
        add_error_inner(s, error, "a binding statement must end with a semicolon.");
        return 0;
    }

    *out = (struct statement) {
        .kind = BINDING_STATEMENT,
        .binding_statement = (struct binding_statement) {
            .variable_name = variable_name,
            .variable_type = type,
            .value = expression,
            .has_type = has_type
        },
        .metadata = metadata
    };

    return 1;
}

int parse_block_statement(struct token_buffer *s,
                          struct statement *out,
                          struct error *error)
{
    struct statement_metadata metadata = statement_metadata(s);
    struct token tmp = {0};
    if (!get_token_type(s, &tmp, OPEN_CURLY_PAREN)) return 0;

    struct list_statement *statements = malloc(sizeof(*statements));
    *statements = list_create(statement, 10);

    for (;;) {
        struct statement statement = {0};
        if (parse_statement(s, &statement, error)) {
            list_append(statements, statement);
        } else {
            return 0;
        }

        if (get_token_type(s, &tmp, CLOSE_CURLY_PAREN)) {
            break;
        }
    }

    *out = (struct statement) {
        .kind = BLOCK_STATEMENT,
        .statements = statements,
        .metadata = metadata
    };

    return 1;
}

int parse_if_statement(struct token_buffer *s,
                       struct statement *out,
                       struct error *error)
{
    struct statement_metadata metadata = statement_metadata(s);
    struct token tmp = {0};
    struct expression condition = {0};
    struct statement *success_statement = malloc(sizeof(*success_statement));
    struct statement *else_statement = NULL;

    if (!get_token_type(s, &tmp, IF_KEYWORD)) return 0;
    if (!get_token_type(s, &tmp, OPEN_ROUND_PAREN)) {
        add_error_inner(s, error, "`if` must be followed by a predicate within round parentheses.");
        return 0;
    }

    if (!parse_expression(s, &condition, error)) return 0;
    if (!get_token_type(s, &tmp, CLOSE_ROUND_PAREN)) {
        add_error_inner(s, error, "missing a closing round parenthesis, `)`.");
        return 0;
    }

    if (!parse_block_statement(s, success_statement, error)) {
        add_error_inner(s, error, "invalid success branch.");
        return 0;
    }

    if (get_token_type(s, &tmp, ELSE_KEYWORD)) {
        else_statement = malloc(sizeof(*else_statement));
        if (!parse_if_statement(s, else_statement, error) &&
            !parse_block_statement(s, else_statement, error))
        {
            add_error_inner(s, error, "invalid else branch.");
            return 0;
        }
    }

    *out = (struct statement) {
        .kind = IF_STATEMENT,
        .if_statement = (struct if_statement) {
            .condition = condition,
            .success_statement = success_statement,
            .else_statement = else_statement
        },
        .metadata = metadata
    };

    return 1;
}

int parse_action_statement(struct token_buffer *s, struct statement *out, struct error *error)
{
    struct statement_metadata metadata = statement_metadata(s);
    struct token tmp = {0};
    struct expression expression = {0};
    if (!parse_expression(s, &expression, error)) return 0;
    if (!get_token_type(s, &tmp, SEMICOLON)) {
        add_error_inner(s, error, "an action statement must end with a semicolon.");
        return 0;
    }
    
    *out = (struct statement) {
        .kind = ACTION_STATEMENT,
        .expression = expression,
        .metadata = metadata
    };

    return 1;
}

int parse_while_loop_statement(struct token_buffer *s, struct statement *out, struct error *error)
{
    struct statement_metadata metadata = statement_metadata(s);
    struct token tmp = {0};
    struct statement *do_statement = malloc(sizeof(*do_statement));
    struct expression expression = {0};

    if (!get_token_type(s, &tmp, WHILE_KEYWORD)) return 0;
    if (!get_token_type(s, &tmp, OPEN_ROUND_PAREN)) {
        add_error_inner(s, error, "`while` must be followed by a predicate within round parentheses.");
        return 0;
    }

    if (!parse_expression(s, &expression, error)) return 0;
    if (!get_token_type(s, &tmp, CLOSE_ROUND_PAREN)) {
        add_error_inner(s, error, "missing a closing round parenthesis, `)`.");
        return 0;
    }

    if (!parse_block_statement(s, do_statement, error)) {
        add_error_inner(s, error, "invalid while block.");
        return 0;
    }

    *out = (struct statement) {
        .kind = WHILE_LOOP_STATEMENT,
        .while_loop_statement = (struct while_loop_statement) {
            .condition = expression,
            .do_statement = do_statement
        },
        .metadata = metadata
    };

    return 1;
}

int parse_type_declaration(struct token_buffer *s,
                           struct statement *out,
                           struct error *error)
{
    struct statement_metadata metadata = statement_metadata(s);
    struct type type = {0};
    
    if (!parse_type(s, &type, 1, 0, error)) return 0;
    if (type.kind != TY_FUNCTION) {
        *out = (struct statement) {
            .kind = TYPE_DECLARATION_STATEMENT,
            .type_declaration = (struct type_declaration_statement) {
                .type = type
            },
            .metadata = metadata
        };
        return 1;
    }

    struct statement body = {0};
    if (!parse_block_statement(s, &body, error)) {
        add_error_inner(s, error, "a function must have a valid body.");
        return 0;
    }

    *out = (struct statement) {
        .kind = TYPE_DECLARATION_STATEMENT,
        .type_declaration = (struct type_declaration_statement) {
            .type = type,
            .statements = body.statements
        },
        .metadata = metadata
    };

    return 1;
}

int parse_case_statement(struct token_buffer *tb, struct case_statement *out, struct error *error)
{
    struct token tmp = {0};
    struct switch_pattern pattern = {0};
    struct statement *s = malloc(sizeof(*s));
    if (!get_token_type(tb, &tmp, CASE_KEYWORD)) return 0;
    if (!parse_switch_pattern(tb, &pattern, error)) return 0;
    if (!get_token_type(tb, &tmp, COLON)) return 0;
    if (!parse_statement(tb, s, error)) return 0;
    *out = (struct case_statement) {
        .pattern = pattern,
        .statement = s
    };
    
    return 1;
}

int parse_switch_statement(struct token_buffer *tb, struct statement *out, struct error *error)
{
    struct statement_metadata metadata = statement_metadata(tb);
    struct token tmp = {0};
    struct expression switch_on = {0};
    struct list_case_statement cases = list_create(case_statement, 10);
    int should_continue = 1;

    if (!get_token_type(tb, &tmp, SWITCH_KEYWORD))    return 0;
    if (!get_token_type(tb, &tmp, OPEN_ROUND_PAREN))  return 0;
    if (!parse_expression(tb, &switch_on, error))     return 0;
    if (!get_token_type(tb, &tmp, CLOSE_ROUND_PAREN)) return 0;
    if (!get_token_type(tb, &tmp, OPEN_CURLY_PAREN))  return 0;

    while (should_continue) {
        struct case_statement case_s = {0};
        if (!parse_case_statement(tb, &case_s, error))       return 0;
        list_append(&cases, case_s);
        should_continue = !get_token_type(tb, &tmp, CLOSE_CURLY_PAREN);
    }

    *out = (struct statement) {
        .kind = SWITCH_STATEMENT,
        .switch_statement = (struct switch_statement) {
            .switch_expression = switch_on,
            .cases = cases
        },
        .metadata = metadata
    };
    
    return 1;
}

int parse_c_block_statement(struct token_buffer *b,
                            struct statement *out,
                            struct error *error)
{
    struct statement_metadata metadata = statement_metadata(b);
    struct token tmp = {0};
    if (!get_token_type(b, &tmp, C_LITERAL)) return 0;

    *out = (struct statement) {
        .kind = C_BLOCK_STATEMENT,
        .c_block_statement = (struct c_block_statement) {
            .raw_c = tmp.identifier
        },
        .metadata = metadata
    };

    return 1;
}

int parse_statement(struct token_buffer *s,
                    struct statement *out,
                    struct error *error)
{
    return try_parse(s, out, error, (parser_t)parse_return_statement)
        || try_parse(s, out, error, (parser_t)parse_binding_statement)
        || try_parse(s, out, error, (parser_t)parse_break_statement)
        || try_parse(s, out, error, (parser_t)parse_action_statement)
        || try_parse(s, out, error, (parser_t)parse_if_statement)
        || try_parse(s, out, error, (parser_t)parse_block_statement)
        || try_parse(s, out, error, (parser_t)parse_while_loop_statement)
        || try_parse(s, out, error, (parser_t)parse_switch_statement)
        || try_parse(s, out, error, (parser_t)parse_c_block_statement);
}

int parse_top_level_statement(struct token_buffer *s,
                              struct statement *out,
                              struct error *error)
{
    return try_parse(s, out, error, (parser_t)parse_type_declaration);
}

int parse_rm_file(struct token_buffer *s,
                  struct list_statement *out,
                  struct error *error)
{
    struct list_statement statements = list_create(statement, 10);

    while (s->current_position < s->size) {
        struct statement statement = {0};
        if (parse_top_level_statement(s, &statement, error)) {
            list_append(&statements, statement);
        } else {
            //add_error_inner(s, error, "not allowed on the top level.");
            return 0;
        }
    }

    *out = statements;
    return 1;
}
