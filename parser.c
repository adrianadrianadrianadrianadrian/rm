#include "list.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TODO(msg) \
    fprintf(stderr, "%s:%d: todo: `%s`\n", __FILE__, __LINE__, msg); \
    exit(1);

typedef struct positional_char {
    char value;
    int row;
    int col;
} positional_char;

LIST(positional_char);
CREATE_LIST(positional_char);
APPEND_LIST(positional_char);

LIST(char);
CREATE_LIST(char);
APPEND_LIST(char);

struct file_buffer {
    struct positional_char *data;
    size_t current_position;
    size_t size;
};

struct file_buffer create_file_buffer(FILE *fstream) {
    #define tmp_buf_size 1024
    static char buffer[tmp_buf_size];
    struct list_positional_char chars = create_list_positional_char(tmp_buf_size);

    int row = 1;
    int col = 1;
    size_t read_amount = 0;

    while ((read_amount = fread(&buffer, sizeof(char), tmp_buf_size, fstream)) > 0)
    {
        for (size_t i = 0; i < read_amount; i++) {
            append_list_positional_char(&chars, (struct positional_char) {
                .value = buffer[i],
                .row = row,
                .col = col
            });

            if (buffer[i] == '\n') {
                row += 1;
                col = 0;
            } else {
                col += 1;
            }
        }
    }

    return (struct file_buffer) {
        .data = chars.data,
        .size = chars.size,
        .current_position = 0,
    };
}

int read_file_buffer(struct file_buffer *b,
                     size_t amount,
                     struct positional_char *out)
{
    if (b->current_position + amount > b->size) {
        return 0;
    }

    assert(b->current_position + amount <= b->size);
    for (size_t i = 0; i < amount; ++i) {
        out[i] = b->data[b->current_position + i];
    }

    b->current_position += amount;
    return 1;
}

void seek_back(struct file_buffer *b, size_t amount) {
    if (b->size == 0) return;
    assert(b->current_position >= amount);
    b->current_position -= amount;
}

void read_until(struct file_buffer *b,
                struct list_char *out,
                int (*p)(char),
                int eat_last)
{
    struct positional_char test;
    for (;;) {
        if (!read_file_buffer(b, 1, &test)) {
            return;
        }

        if (p(test.value)) {
            if (!eat_last) seek_back(b, 1);
            return;
        }

        append_list_char(out, test.value);
    }
}

// tokenisation
enum token_type {
    SEMICOLON = 1,
    COLON,
    IDENTIFIER,
    PAREN_OPEN,
    PAREN_CLOSE,
    ARROW,
    MATH_OPERATOR,
    KEYWORD,
    COMMA,
    PIPE,
    CHAR_LITERAL,
    STR_LITERAL,
    STAR,
    AND,
    NUMERIC
};

enum keyword_type {
    FN = 1,
    ENUM,
    STRUCT,
    IF,
    WHILE,
    RETURN,
    BOOLEAN_TRUE,
    BOOLEAN_FALSE
};

enum paren_type {
    ROUND = 0,
    CURLY,
    SQUARE
};

enum math_op_type {
    EQ = 0,
    BANG,
    GREATER,
    LESS,
    MOD,
    DIV,
    PLUS,
    MINUS
};

typedef struct token {
    enum token_type token_type;
    union {
        struct list_char *identifier;
        enum paren_type paren_type;
        enum math_op_type math_op_type;
        enum keyword_type keyword_type;
        double numeric;
    };
    int position;
} token;

LIST(token);
CREATE_LIST(token);
APPEND_LIST(token);

int is_keyword(struct list_char *ident, enum keyword_type *out) {
    if (strcmp(ident->data, "fn") == 0) {
        *out = FN;
        return 1;
    }

    if (strcmp(ident->data, "enum") == 0) {
        *out = ENUM;
        return 1;
    }

    if (strcmp(ident->data, "struct") == 0) {
        *out = STRUCT;
        return 1;
    }

    if (strcmp(ident->data, "if") == 0) {
        *out = IF;
        return 1;
    }

    if (strcmp(ident->data, "while") == 0) {
        *out = WHILE;
        return 1;
    }

    if (strcmp(ident->data, "return") == 0) {
        *out = RETURN;
        return 1;
    }

    if (strcmp(ident->data, "true") == 0) {
        *out = BOOLEAN_TRUE;
        return 1;
    }

    if (strcmp(ident->data, "false") == 0) {
        *out = BOOLEAN_FALSE;
        return 1;
    }

    return 0;
}

int whitespace(char c) {
    switch (c) {
        case ' ':
        case '\n':
        case '\t':
        case '\r':
            return 1;
        default:
            return 0;
    }
}

int is_double_quote(char c) {
    return c == '"';
}

int is_special_char(char c) {
    switch (c) {
        case ':':
        case ';':
        case '(':
        case ')':
        case '-':
        case '>':
        case '{':
        case '}':
        case '[':
        case ']':
        case '=':
        case '!':
        case '<':
        case '%':
        case '/':
        case ',':
        case '|':
        case '\'':
        case '"':
        case '*':
        case '+':
        case '&':
            return 1;
        default:
            return 0;
    }
}

int is_special_or_whitespace(char c) {
    return is_special_char(c) || whitespace(c);
}

int next_token(struct file_buffer *b, struct token *out) {
    struct positional_char test;
    while (read_file_buffer(b, 1, &test)) {
        switch (test.value) {
            case ':': {
                *out = (struct token) {
                    .token_type = COLON,
                    .position = b->current_position
                };
                return 1;
            }
            case ';': {
                *out = (struct token) {
                    .token_type = SEMICOLON,
                    .position = b->current_position
                };
                return 1;
            }
            case '(': {
                *out = (struct token) {
                    .token_type = PAREN_OPEN,
                    .paren_type = ROUND,
                    .position = b->current_position
                };
                return 1;
            }
            case ')': {
                *out = (struct token) {
                    .token_type = PAREN_CLOSE,
                    .paren_type = ROUND,
                    .position = b->current_position
                };
                return 1;
            }
            case '{': {
                *out = (struct token) {
                    .token_type = PAREN_OPEN,
                    .paren_type = CURLY,
                    .position = b->current_position
                };
                return 1;
            }
            case '}': {
                *out = (struct token) {
                    .token_type = PAREN_CLOSE,
                    .paren_type = CURLY,
                    .position = b->current_position
                };
                return 1;
            }
            case '[': {
                *out = (struct token) {
                    .token_type = PAREN_OPEN,
                    .paren_type = SQUARE,
                    .position = b->current_position
                };
                return 1;
            }
            case ']': {
                *out = (struct token) {
                    .token_type = PAREN_CLOSE,
                    .paren_type = SQUARE,
                    .position = b->current_position
                };
                return 1;
            }
            case '=': {
                *out = (struct token) {
                    .token_type = MATH_OPERATOR,
                    .math_op_type = EQ,
                    .position = b->current_position
                };
                return 1;
            }
            case '!': {
                *out = (struct token) {
                    .token_type = MATH_OPERATOR,
                    .math_op_type = BANG,
                    .position = b->current_position
                };
                return 1;
            }
            case '%': {
                *out = (struct token) {
                    .token_type = MATH_OPERATOR,
                    .math_op_type = MOD,
                    .position = b->current_position
                };
                return 1;
            }
            case '/': {
                *out = (struct token) {
                    .token_type = MATH_OPERATOR,
                    .math_op_type = DIV,
                    .position = b->current_position
                };
                return 1;
            }
            case ',': {
                *out = (struct token) {
                    .token_type = COMMA,
                    .position = b->current_position
                };
                return 1;
            }
            case '|': {
                *out = (struct token) {
                    .token_type = PIPE,
                    .position = b->current_position
                };
                return 1;
            }
            case '*': {
                *out = (struct token) {
                    .token_type = STAR,
                    .position = b->current_position
                };
                return 1;
            }
            case '&': {
                *out = (struct token) {
                    .token_type = AND,
                    .position = b->current_position
                };
                return 1;
            }
            case '\'': {
                int char_start_position = b->current_position;
                struct list_char *c = malloc(sizeof(*c));
                *c = create_list_char(2);
                if (!read_file_buffer(b, 1, &test)) return 0;
                append_list_char(c, test.value);
                if (!read_file_buffer(b, 1, &test)) return 0;
                if (test.value != '\'')             return 0;
                
                *out = (struct token) {
                    .token_type = CHAR_LITERAL,
                    .identifier = c,
                    .position = char_start_position
                };
                return 1;
            }
            case '"': {
                int str_start_position = b->current_position;
                struct list_char *str = malloc(sizeof(*str));
                *str = create_list_char(50);
                read_until(b, str, is_double_quote, 1);
                
                *out = (struct token) {
                    .token_type = STR_LITERAL,
                    .identifier = str,
                    .position = str_start_position
                };
                return 1;
            }
            case '+': {
                *out = (struct token) {
                    .token_type = MATH_OPERATOR,
                    .math_op_type = PLUS,
                    .position = b->current_position
                };
                return 1;
            }
            case '-': {
                int start_position = b->current_position;
                if (read_file_buffer(b, 1, &test)) {
                    if (test.value == '>') {
                        *out = (struct token) {
                            .token_type = ARROW,
                            .position = start_position
                        };
                        return 1;
                    } else {
                        seek_back(b, 1);
                    }
                }

                *out = (struct token) {
                    .token_type = MATH_OPERATOR,
                    .math_op_type = MINUS,
                    .position = b->current_position
                };
                return 1;
            }
            default: {
                if (whitespace(test.value)) {
                    break;
                }

                int start_position = b->current_position;
                struct list_char *ident = malloc(sizeof(*ident));
                *ident = create_list_char(10);
                append_list_char(ident, test.value);
                read_until(b, ident, is_special_or_whitespace, 0);
                append_list_char(ident, '\0');

                enum keyword_type keyword;
                if (is_keyword(ident, &keyword)) {
                    *out = (struct token) {
                        .token_type = KEYWORD,
                        .keyword_type = keyword,
                        .position = start_position
                    };
                } else {
                    char *end = NULL;
                    double parsed = strtod(ident->data, &end);
                    if (end == NULL || *end != ident->data[ident->size]) {
                        *out = (struct token) {
                            .token_type = IDENTIFIER,
                            .identifier = ident,
                            .position = start_position
                        };
                    } else {
                        *out = (struct token) {
                            .token_type = NUMERIC,
                            .numeric = parsed,
                            .position = start_position
                        };
                    }
                }

                return 1;
            }
        }
    }

    return 0;
}

struct token_buffer {
    struct list_token tokens;
    size_t current_position;
    size_t size;
    struct file_buffer *source;
};

struct token_buffer create_token_buffer(struct file_buffer *b) {
    struct list_token tokens = create_list_token(b->size);
    struct token tok;

    while (next_token(b, &tok)) {
        append_list_token(&tokens, tok);
    }

    return (struct token_buffer) {
        .tokens = tokens,
        .size = tokens.size,
        .current_position = 0,
        .source = b
    };
}

int get_token(struct token_buffer *s, struct token *out) {
    if (s->current_position >= s->size) return 0;
    *out = s->tokens.data[s->current_position];
    s->current_position += 1;
    return 1;
}

void seek_back_token(struct token_buffer *s, size_t amount) {
    assert(s->current_position >= amount);
    s->current_position -= amount;
}

int get_token_type(struct token_buffer *s,
                   struct token *out,
                   enum token_type ty)
{
    if (!get_token(s, out)) {
        return 0;
    } else if (out->token_type != ty) {
        seek_back_token(s, 1);
        return 0;
    }

    return 1;
}

int get_token_where(struct token_buffer *s,
                    struct token *out,
                    int (*p)(struct token *))
{
    if (!get_token(s, out)) {
        return 0;
    } else if (!p(out)) {
        seek_back_token(s, 1);
        return 0;
    }

    return 1;
}

int get_and_expect_token(struct token_buffer *s,
                         struct token *out,
                         enum token_type ty)
{
    if (!get_token_type(s, out, ty)) {
        // TODO: handle error
        return 0;
    }

    return 1;
}

int get_and_expect_token_where(struct token_buffer *s,
                               struct token *out,
                               int (*p)(struct token *))
{
    if (!get_token_where(s, out, p)) {
        // TODO: handle error
        return 0;
    }

    return 1;
}

int is_open_curly(struct token *t) {
    return t->token_type == PAREN_OPEN && t->paren_type == CURLY;
}

int is_close_curly(struct token *t) {
    return t->token_type == PAREN_CLOSE && t->paren_type == CURLY;
}

int is_open_round(struct token *t) {
    return t->token_type == PAREN_OPEN && t->paren_type == ROUND;
}

int is_close_round(struct token *t) {
    return t->token_type == PAREN_CLOSE && t->paren_type == ROUND;
}

int is_open_square(struct token *t) {
    return t->token_type == PAREN_OPEN && t->paren_type == SQUARE;
}

int is_close_square(struct token *t) {
    return t->token_type == PAREN_CLOSE && t->paren_type == SQUARE;
}

int is_math_eq(struct token *t) {
    return t->token_type == MATH_OPERATOR && t->math_op_type == EQ;
}

int is_math_bang(struct token *t) {
    return t->token_type == MATH_OPERATOR && t->math_op_type == BANG;
}

int is_math_minus(struct token *t) {
    return t->token_type == MATH_OPERATOR && t->math_op_type == MINUS;
}

int is_math_plus(struct token *t) {
    return t->token_type == MATH_OPERATOR && t->math_op_type == PLUS;
}

int is_return_keyword(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == RETURN;
}

int is_keyword_true(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == BOOLEAN_TRUE;
}

int is_keyword_false(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == BOOLEAN_FALSE;
}

// parsing
enum primitive_type {
    UNIT = 1,
    U8,
    I8
};

int is_primitive(struct list_char *raw, enum primitive_type *out)
{
    if (strcmp(raw->data, "void") == 0) {
        *out = UNIT;
        return 1;
    }

    if (strcmp(raw->data, "u8") == 0) {
        *out = U8;
        return 1;
    }

    if (strcmp(raw->data, "i8") == 0) {
        *out = I8;
        return 1;
    }

    return 0;
}

enum type_kind {
    TY_USER_DEFINED = 1,
    TY_PRIMITIVE,
    TY_STRUCT,
    TY_FUNCTION,
    TY_ENUM
};

typedef struct key_type_pair {
    struct list_char field_name;
    struct type *field_type;
} key_type_pair;

LIST(key_type_pair);
CREATE_LIST(key_type_pair);
APPEND_LIST(key_type_pair);

struct function_type {
    struct list_key_type_pair params;
    struct type *return_type;
};

typedef struct type {
    enum type_kind kind;
    int anonymous;
    struct list_char *name;
    union {
        struct function_type function_type;
        enum primitive_type primitive_type;
        struct list_key_type_pair key_type_pairs;
    };
} type;

struct key_type_pair create_key_type_pair() {
    return (struct key_type_pair) {
        .field_name = create_list_char(10),
        .field_type = malloc(sizeof(struct type))
    };
}

LIST(type);
CREATE_LIST(type);
APPEND_LIST(type);

int parse_type(struct token_buffer *s,
               struct type *out,
               int named_fn,
               int named_struct,
               int named_enum,
               int named_primitive);

int parse_key_type_pairs(struct token_buffer *s, struct list_key_type_pair *out)
{
    struct token tmp = {0};
    int should_continue = 1;

    while (should_continue) {
        struct key_type_pair pair = create_key_type_pair();
        if (!get_and_expect_token(s, &tmp, IDENTIFIER))       return 0;
        pair.field_name = *tmp.identifier;
        if (!get_token_type(s, &tmp, COLON))                  return 0;
        if (!parse_type(s, pair.field_type, 0, 1, 1, 0))      return 0;
        append_list_key_type_pair(out, pair);
        should_continue = get_token_type(s, &tmp, COMMA);
    }

    return 1;
}

int parse_function_type(struct token_buffer *s, struct type *out, int named)
{
    struct token tmp;
    struct token name;
    struct list_key_type_pair params = create_list_key_type_pair(10);
    struct type *return_type = malloc(sizeof(*return_type));

    if (named && !get_and_expect_token(s, &name, IDENTIFIER)) return 0;
    if (!get_and_expect_token_where(s, &tmp, is_open_round))  return 0;
    if (!parse_key_type_pairs(s, &params))                    return 0;
    if (!get_and_expect_token_where(s, &tmp, is_close_round)) return 0;
    if (!get_and_expect_token(s, &tmp, ARROW))                return 0;
    if (!parse_type(s, return_type, 0, 0, 0, 0))              return 0;

    *out = (struct type) {
        .kind = TY_FUNCTION,
        .function_type = (struct function_type) {
            .params = params,
            .return_type = return_type
        },
        .anonymous = !named,
        .name = name.identifier
    };

    return 1;
}

int parse_struct_type(struct token_buffer *s, struct type *out, int named)
{
    struct token name = {0};
    struct token tmp = {0};
    struct list_key_type_pair pairs = create_list_key_type_pair(10);

    if (named && !get_and_expect_token(s, &name, IDENTIFIER)) return 0;
    if (!get_and_expect_token_where(s, &tmp, is_open_curly))  return 0;
    if (!parse_key_type_pairs(s, &pairs))                     return 0;
    if (!get_and_expect_token_where(s, &tmp, is_close_curly)) return 0;

    *out = (struct type) {
        .kind = TY_STRUCT,
        .key_type_pairs = pairs,
        .name = name.identifier,
        .anonymous = !named
    };

    return 1;
}

int parse_enum_type(struct token_buffer *s, struct type *out, int named)
{
    struct token name = {0};
    struct token tmp = {0};
    struct list_key_type_pair pairs = create_list_key_type_pair(10);

    if (named && !get_and_expect_token(s, &name, IDENTIFIER)) return 0;
    if (!get_and_expect_token_where(s, &tmp, is_open_curly))  return 0;
    if (!parse_key_type_pairs(s, &pairs))                     return 0;
    if (!get_and_expect_token_where(s, &tmp, is_close_curly)) return 0;

    *out = (struct type) {
        .kind = TY_ENUM,
        .key_type_pairs = pairs,
        .name = name.identifier,
        .anonymous = !named
    };

    return 1;
}

int parse_primitive_type(struct token_buffer *s, struct type *out, int named)
{
    struct token tmp = {0};
    enum primitive_type primitive_type;
    struct token name = {0};

    if (named && !get_and_expect_token(s, &name, IDENTIFIER)) return 0;
    if (named && !get_and_expect_token(s, &tmp, COLON))       return 0;
    if (!get_and_expect_token(s, &tmp, IDENTIFIER))           return 0;
    if (!is_primitive(tmp.identifier, &primitive_type))       return 0;

    *out = (struct type) {
        .kind = TY_PRIMITIVE,
        .primitive_type = primitive_type,
        .anonymous = !named,
        .name = name.identifier
    };

    return 1;
}

int parse_type(struct token_buffer *s,
               struct type *out,
               int named_fn,
               int named_struct,
               int named_enum,
               int named_primitive)
{
    struct token tmp = {0};
    if (get_token_type(s, &tmp, KEYWORD)) {
        switch (tmp.keyword_type) {
            case FN:
                return parse_function_type(s, out, named_fn);
            case ENUM:
                return parse_enum_type(s, out, named_enum);
            case STRUCT:
                return parse_struct_type(s, out, named_struct);
            default:
                return 0;
        }
    }

    return parse_primitive_type(s, out, named_primitive);
}

enum expression_kind {
    LITERAL_EXPRESSION = 1,
    UNARY_EXPRESSION,
    BINARY_EXPRESSION,
    GROUP_EXPRESSION,
    FUNCTION_EXPRESSION
};

enum literal_expression_kind {
    LITERAL_BOOLEAN = 1,
    LITERAL_CHAR,
    LITERAL_STR,
    LITERAL_NUMERIC
};

struct literal_expression {
    enum literal_expression_kind kind;
    union {
        int boolean;
        char character;
        struct list_char *str;
        double numeric;
    };
};

enum binary_operator {
    PLUS_BINARY = 1,
    OR_BINARY,
    AND_BINARY,
    BITWISE_OR_BINARY,
    BITWISE_AND_BINARY
};

struct binary_expression {
    enum binary_operator binary_op;
    struct expression *l;
    struct expression *r;
};

enum unary_operator {
    BANG_UNARY = 1,
    STAR_UNARY,
    MINUS_UNARY
};

struct unary_expression {
    enum unary_operator operator;
    struct expression *expression;
};

struct function_expression {
    struct list_char *function_name;
    struct list_expression *params;
};

typedef struct expression {
    enum expression_kind kind;
    union {
        struct unary_expression unary;
        struct literal_expression literal;
        struct expression *grouped;
        struct binary_expression binary;
        struct function_expression function;
    };
} expression;

LIST(expression);
CREATE_LIST(expression);
APPEND_LIST(expression);

int parse_expression(struct token_buffer *s, struct expression *out);

int parse_boolean_literal_expression(struct token_buffer *s,
                                     struct literal_expression *out)
{
    struct token tmp = {0};
    if (get_and_expect_token_where(s, &tmp, is_keyword_true)) {
        *out = (struct literal_expression) {
            .kind = LITERAL_BOOLEAN,
            .boolean = 1
        };
        return 1;
    }

    if (get_and_expect_token_where(s, &tmp, is_keyword_false)) {
        *out = (struct literal_expression) {
            .kind = LITERAL_BOOLEAN,
            .boolean = 0
        };
        return 1;
    }

    seek_back_token(s, 1);
    return 0;
}

int parse_char_literal_expression(struct token_buffer *s,
                                  struct literal_expression *out)
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
                                     struct literal_expression *out)
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
                                 struct literal_expression *out)
{
    struct token tmp = {0};
    if (!get_token_type(s, &tmp, STR_LITERAL)) return 0;

    *out = (struct literal_expression) {
        .kind = LITERAL_STR,
        .str = tmp.identifier
    };

    return 1;
}

int parse_literal_expression(struct token_buffer *s,
                             struct literal_expression *out)
{
    return parse_char_literal_expression(s, out)
        || parse_str_literal_expression(s, out)
        || parse_numeric_literal_expression(s, out)
        || parse_boolean_literal_expression(s, out);
}

int parse_unary_operator(struct token_buffer *s,
                         enum unary_operator *out)
{
    struct token tmp = {0};
    if (get_token_where(s, &tmp, is_math_bang)) {
        *out = BANG_UNARY;
        return 1;
    }

    if (get_token_type(s, &tmp, STAR)) {
        *out = STAR_UNARY;
        return 1;
    }

    if (get_token_where(s, &tmp, is_math_minus)) {
        *out = MINUS_UNARY;
        return 1;
    }

    return 0;
}

int parse_binary_operator(struct token_buffer *s, enum binary_operator *out)
{
    struct token tmp = {0};
    if (get_token_where(s, &tmp, is_math_plus)) {
        *out = PLUS_BINARY;
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

    return 0;
}

int parse_function_expression(struct token_buffer *s, struct function_expression *out)
{
    struct token tmp = {0};
    struct token name = {0};
    if (!get_token_type(s, &name, IDENTIFIER))    return 0;
    if (!get_token_where(s, &tmp, is_open_round)) return 0;

    struct list_expression *params = malloc(sizeof(*params));
    *params = create_list_expression(10);
    int should_continue = 1;
    int success = 0;

    while (should_continue) {
        struct expression expr = {0};
        if (parse_expression(s, &expr)) {
            append_list_expression(params, expr);
            success = 1;
        }
        should_continue = get_token_type(s, &tmp, COMMA);
    }

    if (!get_token_where(s, &tmp, is_close_round)) return 0;
    if (!success)                                  return 0;

    *out = (struct function_expression) {
        .function_name = name.identifier,
        .params = params
    };

    return 1;
}

int parse_single_expression(struct token_buffer *s, struct expression *out) {
    struct token tmp = {0};
    enum unary_operator unary_op;

    if (parse_unary_operator(s, &unary_op)) {
        struct expression *nested = malloc(sizeof(*nested));
        if (!parse_single_expression(s, nested)) return 0;

        *out = (struct expression) {
            .kind = UNARY_EXPRESSION,
            .unary = (struct unary_expression) {
                .operator = unary_op,
                .expression = nested
            }
        };
        return 1;
    }

    if (get_token_where(s, &tmp, is_open_round)) {
        struct expression *nested = malloc(sizeof(*nested));
        if (!parse_expression(s, nested))              return 0;
        if (!get_token_where(s, &tmp, is_close_round)) return 0;
        *out = (struct expression) {
            .kind = GROUP_EXPRESSION,
            .grouped = nested
        };
        return 1;
    }

    struct function_expression function = {0};
    if (parse_function_expression(s, &function)) {
        *out = (struct expression) {
            .kind = FUNCTION_EXPRESSION,
            .function = function
        };
        return 1;
    }

    struct literal_expression literal = {0};
    if (parse_literal_expression(s, &literal)) {
        *out = (struct expression) {
            .kind = LITERAL_EXPRESSION,
            .literal = literal
        };
        return 1;
    }

    return 0;
}

int parse_expression(struct token_buffer *s, struct expression *out)
{
    enum binary_operator op;
    int parsed_left = 0;
    struct expression *l = malloc(sizeof(*l));
    struct expression *r = malloc(sizeof(*r));

    if (parse_single_expression(s, l)) {
        parsed_left = 1;
    } else {
        return 0;
    }

    if (parse_binary_operator(s, &op) && parse_expression(s, r)) {
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

enum statement_kind {
    BINDING_STATEMENT = 1,
    IF_STATEMENT,
    RETURN_STATEMENT
};

struct binding_statement {
    struct list_char variable_name;
    struct type variable_type;
    struct expression value;
};

typedef struct statement {
    enum statement_kind kind;
    union {
        struct expression return_expression;
        struct binding_statement binding_statement;
    };
} statement;

int parse_return_statement(struct token_buffer *s, struct expression *out)
{
    struct token tmp = {0};
    if (!get_and_expect_token_where(s, &tmp, is_return_keyword)) return 0;
    if (!parse_expression(s, out))                               return 0;
    if (!get_and_expect_token(s, &tmp, SEMICOLON))               return 0;
    return 1;
}

int parse_binding_statement(struct token_buffer *s, struct binding_statement *out)
{
    struct token tmp = {0};
    struct type type = {0};
    struct expression expression = {0};
    struct list_char variable_name = {0};

    if (!get_and_expect_token(s, &tmp, IDENTIFIER))       return 0;
    variable_name = *tmp.identifier;
    if (!get_and_expect_token(s, &tmp, COLON))            return 0;
    if (!parse_type(s, &type, 0, 0, 0, 0))                return 0;
    if (!get_and_expect_token_where(s, &tmp, is_math_eq)) return 0;
    if (!parse_expression(s, &expression))                return 0;
    if (!get_and_expect_token(s, &tmp, SEMICOLON))        return 0;

    *out = (struct binding_statement) {
        .variable_name = variable_name,
        .variable_type = type,
        .value = expression
    };

    return 1;
}

// C generation
//void write_type(struct type *ty);

// char *primitive_c_type(enum primitive_type ty) {
//     switch (ty) {
//         case UNIT:
//             return "void";
//         case U8:
//             return "char";
//         case I8:
//             return "int";
//     }
// }
//
// void write_function_type(struct type *ty) {
//     assert(ty->kind == TY_FUNCTION);
//     write_type(ty->function_type.return_type);
//     printf(" %s(", ty->name->data);
//     for (size_t i = 0; i < ty->function_type.params->size; i++) {
//         struct type type = ty->function_type.params->data[i];
//         write_type(&type);
//         if (i < ty->function_type.params->size - 1) {
//             printf(", ");
//         }
//     }
//     printf(")");
// }
//
// void write_struct(struct type *ty) {
//     assert(ty->kind == TY_STRUCT);
//     printf("struct {");
//     //write_type(ty->struct_type.field_type);
//     printf("}");
// }
//
// void write_primitive(struct type *ty) {
//     assert(ty->kind == TY_PRIMITIVE);
//     printf("%s", primitive_c_type(ty->primitive_type));
//     if (!ty->anonymous) {
//         printf(" %s", ty->name->data);
//     }
// }
//
// void write_user_defined(struct type *ty) {
//     assert(ty->kind == TY_USER_DEFINED);
//     printf("%s", ty->user_defined.data);
//     if (!ty->anonymous) {
//         printf(" %s", ty->name->data);
//     }
// }
//
// void write_type(struct type *ty) {
//     switch (ty->kind) {
//         case TY_PRIMITIVE:
//             write_primitive(ty);
//             break;
//         case TY_STRUCT:
//             write_struct(ty);
//             break;
//         case TY_FUNCTION:
//             write_function_type(ty);
//             break;
//         case TY_ENUM:
//             break;
//         case TY_USER_DEFINED:
//             write_user_defined(ty);
//             break;
//     }
// }
