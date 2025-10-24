#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"

#define parser_t int (*)(struct token_buffer *, void *, struct parse_error *error)
int try_parse(struct token_buffer *s,
              void *out,
              struct parse_error *error,
              int (*parser)(struct token_buffer *, void *, struct parse_error *error))
{
    size_t start = s->current_position;
    if (!parser(s, out, error)) {
        seek_back_token(s, s->current_position - start);
        return 0;
    }
    
    return 1;
}

int is_primitive(struct list_char *raw, enum primitive_type *out)
{
    if (strcmp(raw->data, "void") == 0) {
        *out = UNIT;
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

    return 0;
}

struct key_type_pair create_key_type_pair() {
    return (struct key_type_pair) {
        .field_name = list_create(char, 10),
        .field_type = malloc(sizeof(struct type))
    };
}


int parse_pointer_type_modifier(struct token_buffer *tb, struct type_modifier *out, struct parse_error *error)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, STAR)) return 0;
    *out = (struct type_modifier) {
        .kind = POINTER_MODIFIER_KIND,
    };

    return 1;
}

int parse_nullable_type_modifier(struct token_buffer *tb, struct type_modifier *out, struct parse_error *error)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, QUESTION_MARK)) return 0;
    *out = (struct type_modifier) {
        .kind = NULLABLE_MODIFIER_KIND,
    };

    return 1;
}

int parse_array_type_modifier(struct token_buffer *tb, struct type_modifier *out, struct parse_error *error)
{
    struct token tmp = {0};
    int size = 0;
    int sized = 0;
    if (!get_token_type(tb, &tmp, OPEN_SQUARE_PAREN))  return 0;
    if (get_token_type(tb, &tmp, NUMERIC)) {
        sized = 1;
        size = (int)tmp.numeric; // TODO: numeric token needs improving
    }
    if (!get_token_type(tb, &tmp, CLOSE_SQUARE_PAREN)) return 0;
    *out = (struct type_modifier) {
        .kind = ARRAY_MODIFIER_KIND,
        .array_modifier = (struct array_type_modifier) {
            .size = size,
            .sized = sized
        }
    };

    return 1;
}

int parse_mutable_type_modifier(struct token_buffer *tb, struct type_modifier *out, struct parse_error *error)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, MUTABLE_KEYWORD)) return 0;
    *out = (struct type_modifier) {
        .kind = MUTABLE_MODIFIER_KIND,
    };

    return 1;
}

int parse_type_modifier(struct token_buffer *tb, struct type_modifier *out, struct parse_error *error)
{
    return try_parse(tb, out, error, (parser_t)parse_pointer_type_modifier)
        || try_parse(tb, out, error, (parser_t)parse_nullable_type_modifier)
        || try_parse(tb, out, error, (parser_t)parse_array_type_modifier)
        || try_parse(tb, out, error, (parser_t)parse_mutable_type_modifier);
}

struct list_type_modifier parse_modifiers(struct token_buffer *tb, struct parse_error *error)
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
               struct parse_error *error);

int parse_key_type_pairs(struct token_buffer *s,
                         struct list_key_type_pair *out,
                         struct parse_error *error)
{
    struct token tmp = {0};
    int should_continue = 1;

    while (should_continue) {
        struct key_type_pair pair = create_key_type_pair();
        if (!get_token_type(s, &tmp, IDENTIFIER))  return 0;
        pair.field_name = *tmp.identifier;
        if (!get_token_type(s, &tmp, COLON))       return 0;
        if (!parse_type(s, pair.field_type, 0, 1, error)) return 0;
        list_append(out, pair);
        should_continue = get_token_type(s, &tmp, COMMA);
    }

    return 1;
}

int parse_function_type(struct token_buffer *s, struct type *out, int named, struct parse_error *error)
{
    struct token tmp;
    struct token name;
    struct list_key_type_pair params = list_create(key_type_pair, 10);
    struct type *return_type = malloc(sizeof(*return_type));

    if (named && !get_token_type(s, &name, IDENTIFIER)) return 0;
    if (!get_token_type(s, &tmp, OPEN_ROUND_PAREN))     return 0;
    parse_key_type_pairs(s, &params, error);
    if (!get_token_type(s, &tmp, CLOSE_ROUND_PAREN))    return 0;
    if (!get_token_type(s, &tmp, RIGHT_ARROW))          return 0;
    if (!parse_type(s, return_type, 0, 1, error))       return 0;

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

int parse_struct_type(struct token_buffer *s, struct type *out, int predefined_type, struct parse_error *error)
{
    struct token name = {0};
    struct token tmp = {0};
    struct list_key_type_pair pairs = list_create(key_type_pair, 10);

    if (!get_token_type(s, &name, IDENTIFIER))           return 0;
    if (!predefined_type && get_token_type(s, &tmp, OPEN_CURLY_PAREN)) {
        if (!parse_key_type_pairs(s, &pairs, error))     return 0;
        if (!get_token_type(s, &tmp, CLOSE_CURLY_PAREN)) return 0;
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

int parse_enum_type(struct token_buffer *s, struct type *out, int predefined_type, struct parse_error *error)
{
    struct token name = {0};
    struct token tmp = {0};
    struct list_key_type_pair pairs = list_create(key_type_pair, 10);

    if (!get_token_type(s, &name, IDENTIFIER))              return 0;
    if (!predefined_type && get_token_type(s, &tmp, OPEN_CURLY_PAREN)) {
        if (!parse_key_type_pairs(s, &pairs, error))        return 0;
        if (!get_token_type(s, &tmp, CLOSE_CURLY_PAREN))    return 0;
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

int parse_primitive_type(struct token_buffer *s, struct type *out, struct parse_error *error)
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
               struct parse_error *error)
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

int parse_expression(struct token_buffer *s, struct expression *out, struct parse_error *error);

int parse_boolean_literal_expression(struct token_buffer *s,
                                     struct literal_expression *out,
                                     struct parse_error *error)
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
                                  struct parse_error *error)
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
                                     struct parse_error *error)
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
                                 struct parse_error *error)
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
                                        struct parse_error *error)
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
                                         struct parse_error *error)
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

    if (!get_token_type(s, &name, IDENTIFIER))       return 0;
    if (!get_token_type(s, &tmp, OPEN_CURLY_PAREN))  return 0;

    struct list_key_expression pairs = list_create(key_expression, 10);
    int should_continue = 1;
    while (should_continue) {
        struct key_expression pair = {0};
        if (!get_token_type(s, &tmp, IDENTIFIER))    return 0;
        pair.key = tmp.identifier;
        if (!get_token_type(s, &tmp, EQ))            return 0;
        struct expression *e = malloc(sizeof(*e));
        if (!parse_expression(s, e, error))                 return 0;
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
                                  struct parse_error *error)
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
                             struct parse_error *error)
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

int parse_binary_operator(struct token_buffer *s, enum binary_operator *out, struct parse_error *error)
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

    if (get_token_type(s, &tmp, DOT)) {
        *out = DOT_BINARY;
        return 1;
    }

    return 0;
}

int parse_function_expression(struct token_buffer *s, struct function_expression *out, struct parse_error *error)
{
    struct token tmp = {0};
    struct token name = {0};
    if (!get_token_type(s, &name, IDENTIFIER))       return 0;
    if (!get_token_type(s, &tmp, OPEN_ROUND_PAREN))  return 0;

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

    if (!get_token_type(s, &tmp, CLOSE_ROUND_PAREN)) return 0;

    *out = (struct function_expression) {
        .function_name = name.identifier,
        .params = params
    };

    return 1;
}

int parse_expression_inner(struct token_buffer *s, struct expression *out, struct parse_error *error) {
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
        if (!parse_expression(s, nested, error))                return 0;
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

int parse_expression(struct token_buffer *s, struct expression *out, struct parse_error *error) {
    enum binary_operator op;
    int parsed_left = 0;
    struct expression *l = malloc(sizeof(*l));
    struct expression *r = malloc(sizeof(*r));

    if (parse_expression_inner(s, l, error)) {
        parsed_left = 1;
    } else {
        return 0;
    }

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

int parse_switch_pattern(struct token_buffer *tb, struct switch_pattern *out, struct parse_error *error);

int parse_variable_or_underscore_pattern(struct token_buffer *tb, struct switch_pattern *out, struct parse_error *error)
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

int parse_string_pattern(struct token_buffer *tb, struct switch_pattern *out, struct parse_error *error)
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

int parse_number_pattern(struct token_buffer *tb, struct switch_pattern *out, struct parse_error *error)
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

int parse_rest_pattern(struct token_buffer *tb, struct switch_pattern *out, struct parse_error *error)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, DOT)) return 0;
    if (!get_token_type(tb, &tmp, DOT)) return 0;

    *out = (struct switch_pattern) {
        .switch_pattern_kind = REST_PATTERN_KIND
    };
    
    return 1;
}

int parse_array_pattern(struct token_buffer *tb, struct switch_pattern *out, struct parse_error *error)
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

int parse_key_pattern_pair(struct token_buffer *tb, struct key_pattern_pair *out, struct parse_error *error)
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
    if (!get_token_type(tb, &tmp, COLON))     return 0;
    if (parse_switch_pattern(tb, pattern, error))    return 0;
    *out = (struct key_pattern_pair) {
        .key = key,
        .pattern = pattern
    };

    return 0;
}

int parse_object_pattern(struct token_buffer *tb, struct switch_pattern *out, struct parse_error *error)
{
    struct token tmp = {0};
    int should_continue = 1;
    struct list_key_pattern_pair pairs = list_create(key_pattern_pair, 10);

    if (!get_token_type(tb, &tmp, OPEN_CURLY_PAREN))  return 0;
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

int parse_switch_pattern(struct token_buffer *tb, struct switch_pattern *out, struct parse_error *error)
{
    return try_parse(tb, out, error, (parser_t)parse_object_pattern)
        || try_parse(tb, out, error, (parser_t)parse_array_pattern)
        || try_parse(tb, out, error, (parser_t)parse_rest_pattern)
        || try_parse(tb, out, error, (parser_t)parse_number_pattern)
        || try_parse(tb, out, error, (parser_t)parse_string_pattern)
        || try_parse(tb, out, error, (parser_t)parse_variable_or_underscore_pattern);
}

int parse_statement(struct token_buffer *s, struct statement *out, struct parse_error *error);

int parse_include_statement(struct token_buffer *s, struct statement *out, struct parse_error *error)
{
    int external = 0;
    struct token tmp = {0};
    struct list_char raw_include = list_create(char, 10);

    if (!get_token_type(s, &tmp, HASH))            return 0;
    if (!get_token_type(s, &tmp, IDENTIFIER))      return 0;
    if (get_token_type(s, &tmp, LEFT_ARROW)
        && get_token_type(s, &tmp, IDENTIFIER))
    {
        external = 1;
        copy_list_char(&raw_include, tmp.identifier);
        if (!get_token_type(s, &tmp, DOT))         return 0;
        if (!get_token_type(s, &tmp, IDENTIFIER))  return 0;
        list_append(&raw_include, '.');
        copy_list_char(&raw_include, tmp.identifier);
        if (!get_token_type(s, &tmp, RIGHT_ARROW)) return 0;
    } else if (get_token_type(s, &tmp, STR_LITERAL)) {
        raw_include = *tmp.identifier;
    } else {
        return 0;
    }
    
    *out = (struct statement) {
        .kind = INCLUDE_STATEMENT,
        .include_statement = (struct include_statement) {
            .include = raw_include,
            .external = external
        }
    };

    return 1;
}

int parse_break_statement(struct token_buffer *s, struct statement *out, struct parse_error *error)
{
    struct token tmp = {0};
    if (!get_token_type(s, &tmp, BREAK_KEYWORD)) return 0;
    if (!get_token_type(s, &tmp, SEMICOLON))     return 0;

    *out = (struct statement) {
        .kind = BREAK_STATEMENT
    };

    return 1;
}

int parse_return_statement(struct token_buffer *s, struct statement *out, struct parse_error *error)
{
    struct token tmp = {0};
    struct expression expression = {0};
    if (!get_token_type(s, &tmp, RETURN_KEYWORD))  return 0;
    if (!parse_expression(s, &expression, error))  return 0;
    if (!get_token_type(s, &tmp, SEMICOLON))       return 0;

    *out = (struct statement) {
        .kind = RETURN_STATEMENT,
        .expression = expression
    };

    return 1;
}

void add_parse_error(struct token_buffer *s,
                      struct parse_error *out,
                      char *message)
{
    struct token_metadata *tok_metadata = get_token_metadata(s, s->current_position - 1);
    struct list_char error_message = list_create(char, 35);
    append_list_char_slice(&error_message, message);
    struct parse_error err = (struct parse_error) {
        .token_metadata = tok_metadata,
        .errored = 1,
        .error_message = error_message,
        .parent = NULL
    };

    // if (out->parent == NULL && !out->errored) {
    //     struct parse_error *boxed = malloc(sizeof(*boxed));
    //     *boxed = err;
    //     out->parent = boxed;
    //     return;
    // }
        
    *out = err;
}

int parse_binding_statement(struct token_buffer *s,
                            struct statement *out,
                            struct parse_error *error)
{
    struct token tmp = {0};
    struct type type = {0};
    struct expression expression = {0};
    struct list_char variable_name = {0};
    int has_type = 0;
    
    if (!get_token_type(s, &tmp, IDENTIFIER)) return 0;
    variable_name = *tmp.identifier;

    if (get_token_type(s, &tmp, COLON)) {
        if (!parse_type(s, &type, 0, 1, error)) {
            add_parse_error(s, error, "a type annotation is required after a `:` in a binding statement.");
            return 0;
        }
        has_type = 1;
    }

    if (!get_token_type(s, &tmp, EQ)) {
        add_parse_error(s, error, "expected a `=`");
        return 0;
    }

    if (!parse_expression(s, &expression, error)) {
        struct list_char msg = list_create(char, 50);
        append_list_char_slice(&msg, "the variable `");
        append_list_char_slice(&msg, variable_name.data);
        append_list_char_slice(&msg, "` must be bound to a valid expression.");
        append_list_char_slice(&msg, "\0");
        add_parse_error(s, error, msg.data);
        return 0; 
    }

    if (!get_token_type(s, &tmp, SEMICOLON)) {
        add_parse_error(s, error, "a statement must end with a semicolon.");
        return 0;
    }

    *out = (struct statement) {
        .kind = BINDING_STATEMENT,
        .binding_statement = (struct binding_statement) {
            .variable_name = variable_name,
            .variable_type = type,
            .value = expression,
            .has_type = has_type
        }
    };

    return 1;
}

int parse_block_statement(struct token_buffer *s,
                          struct statement *out,
                          struct parse_error *error)
{
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
        .statements = statements
    };

    return 1;
}

int parse_if_statement(struct token_buffer *s,
                       struct statement *out,
                       struct parse_error *error)
{
    struct token tmp = {0};
    struct expression condition = {0};
    struct statement *success_statement = malloc(sizeof(*success_statement));
    struct statement *else_statement = NULL;

    if (!get_token_type(s, &tmp, IF_KEYWORD))         return 0;
    if (!get_token_type(s, &tmp, OPEN_ROUND_PAREN))   return 0;
    if (!parse_expression(s, &condition, error))             return 0;
    if (!get_token_type(s, &tmp, CLOSE_ROUND_PAREN))  return 0;
    if (!parse_block_statement(s, success_statement, error)) return 0;
    if (get_token_type(s, &tmp, ELSE_KEYWORD)) {
        else_statement = malloc(sizeof(*else_statement));
        if (!parse_if_statement(s, else_statement, error) &&
            !parse_block_statement(s, else_statement, error))
        {
            return 0;
        }
    }

    *out = (struct statement) {
        .kind = IF_STATEMENT,
        .if_statement = (struct if_statement) {
            .condition = condition,
            .success_statement = success_statement,
            .else_statement = else_statement
        }
    };

    return 1;
}

int parse_action_statement(struct token_buffer *s, struct statement *out, struct parse_error *error)
{
    struct token tmp = {0};
    struct expression expression = {0};
    if (!parse_expression(s, &expression, error))   return 0;
    if (!get_token_type(s, &tmp, SEMICOLON)) return 0;
    
    *out = (struct statement) {
        .kind = ACTION_STATEMENT,
        .expression = expression
    };

    return 1;
}

int parse_while_loop_statement(struct token_buffer *s, struct statement *out, struct parse_error *error)
{
    struct token tmp = {0};
    struct statement *do_statement = malloc(sizeof(*do_statement));
    struct expression expression = {0};

    if (!get_token_type(s, &tmp, WHILE_KEYWORD))     return 0;
    if (!get_token_type(s, &tmp, OPEN_ROUND_PAREN))  return 0;
    if (!parse_expression(s, &expression, error))           return 0;
    if (!get_token_type(s, &tmp, CLOSE_ROUND_PAREN)) return 0;
    if (!parse_block_statement(s, do_statement, error))     return 0;

    *out = (struct statement) {
        .kind = WHILE_LOOP_STATEMENT,
        .while_loop_statement = (struct while_loop_statement) {
            .condition = expression,
            .do_statement = do_statement
        }
    };

    return 1;
}

int parse_type_declaration(struct token_buffer *s, struct statement *out, struct parse_error *error) {
    struct type type = {0};
    
    if (!parse_type(s, &type, 1, 0, error)) return 0;
    if (type.kind != TY_FUNCTION) {
        *out = (struct statement) {
            .kind = TYPE_DECLARATION_STATEMENT,
            .type_declaration = (struct type_declaration_statement) {
                .type = type
            }
        };
        return 1;
    }

    struct statement body = {0};
    if (!parse_block_statement(s, &body, error)) return 0;

    *out = (struct statement) {
        .kind = TYPE_DECLARATION_STATEMENT,
        .type_declaration = (struct type_declaration_statement) {
            .type = type,
            .statements = body.statements
        }
    };

    return 1;
}

int parse_case_statement(struct token_buffer *tb, struct case_statement *out, struct parse_error *error)
{
    struct token tmp = {0};
    struct switch_pattern pattern = {0};
    struct statement *s = malloc(sizeof(*s));
    if (!get_token_type(tb, &tmp, CASE_KEYWORD)) return 0;
    if (!parse_switch_pattern(tb, &pattern, error))     return 0;
    if (!get_token_type(tb, &tmp, COLON))        return 0;
    if (!parse_statement(tb, s, error))                 return 0;
    *out = (struct case_statement) {
        .pattern = pattern,
        .statement = s
    };
    
    return 1;
}

int parse_switch_statement(struct token_buffer *tb, struct statement *out, struct parse_error *error)
{
    struct token tmp = {0};
    struct expression switch_on = {0};
    struct list_case_statement cases = list_create(case_statement, 10);
    int should_continue = 1;

    if (!get_token_type(tb, &tmp, SWITCH_KEYWORD))    return 0;
    if (!get_token_type(tb, &tmp, OPEN_ROUND_PAREN))  return 0;
    if (!parse_expression(tb, &switch_on, error))            return 0;
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
        }
    };
    
    return 1;
}

int parse_statement(struct token_buffer *s,
                    struct statement *out,
                    struct parse_error *error)
{
    return try_parse(s, out, error, (parser_t)parse_return_statement)
        || try_parse(s, out, error, (parser_t)parse_break_statement)
        || try_parse(s, out, error, (parser_t)parse_action_statement)
        || try_parse(s, out, error, (parser_t)parse_binding_statement)
        || try_parse(s, out, error, (parser_t)parse_if_statement)
        || try_parse(s, out, error, (parser_t)parse_block_statement)
        || try_parse(s, out, error, (parser_t)parse_while_loop_statement)
        || try_parse(s, out, error, (parser_t)parse_switch_statement);
}

int parse_top_level_statement(struct token_buffer *s,
                              struct statement *out,
                              struct parse_error *error)
{
    return try_parse(s, out, error, (parser_t)parse_type_declaration)
        || try_parse(s, out, error, (parser_t)parse_include_statement);
}

int parse_rm_file(struct token_buffer *s,
                  struct list_statement *out,
                  struct parse_error *error)
{
    struct list_statement statements = list_create(statement, 10);

    for (;;) {
        struct statement statement = {0};
        if (parse_top_level_statement(s, &statement, error)) {
            list_append(&statements, statement);
        } else {
            break;
        }
    }

    *out = statements;
    return !error->errored;
}
