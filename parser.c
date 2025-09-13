#include "list.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TODO(msg) \
    fprintf(stderr, "%s:%d: todo: `%s`\n", __FILE__, __LINE__, msg); \
    exit(1);

#define UNREACHABLE(msg) \
    fprintf(stderr, "%s:%d: unreachable: `%s`\n", __FILE__, __LINE__, msg); \
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
    RIGHT_ARROW,
    LEFT_ARROW,
    MATH_OPERATOR,
    KEYWORD,
    COMMA,
    PIPE,
    CHAR_LITERAL,
    STR_LITERAL,
    STAR,
    AND,
    NUMERIC,
    HASH
};

enum keyword_type {
    FN = 1,
    ENUM,
    STRUCT,
    IF,
    WHILE,
    RETURN,
    BOOLEAN_TRUE,
    BOOLEAN_FALSE,
    ELSE,
    BREAK
};

enum paren_type {
    ROUND = 0,
    CURLY,
    SQUARE
};

enum math_op_type {
    EQ = 0,
    BANG,
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

    if (strcmp(ident->data, "break") == 0) {
        *out = BREAK;
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

    if (strcmp(ident->data, "else") == 0) {
        *out = ELSE;
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
        case '#':
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
            case '#': {
                *out = (struct token) {
                    .token_type = HASH,
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
            case '>': {
                *out = (struct token) {
                    .token_type = RIGHT_ARROW,
                    .position = b->current_position
                };
                return 1;
            }
            case '<': {
                *out = (struct token) {
                    .token_type = LEFT_ARROW,
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
                            .token_type = RIGHT_ARROW,
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

int is_break_keyword(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == BREAK;
}

int is_keyword_true(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == BOOLEAN_TRUE;
}

int is_keyword_false(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == BOOLEAN_FALSE;
}

int is_keyword_if(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == IF;
}

int is_keyword_else(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == ELSE;
}

int is_keyword_while(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == WHILE;
}

// parsing
enum primitive_type {
    UNIT = 1,
    BOOL,
    U8,
    I8,
    I16,
    U16,
    I32,
    U32,
    I64,
    U64,
    USIZE,
    F32,
    F64
};

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

enum type_kind {
    TY_PRIMITIVE = 1,
    TY_STRUCT,
    TY_FUNCTION,
    TY_ENUM,
    TY_USER_DEFINED
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
    int pointer_count;
    union {
        struct function_type function_type;
        enum primitive_type primitive_type;
        struct list_key_type_pair key_type_pairs;
        struct list_char *user_defined;
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
        if (!get_token_type(s, &tmp, IDENTIFIER))        return 0;
        pair.field_name = *tmp.identifier;
        if (!get_token_type(s, &tmp, COLON))             return 0;
        if (!parse_type(s, pair.field_type, 0, 1, 1, 0)) return 0;
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
    parse_key_type_pairs(s, &params);
    if (!get_and_expect_token_where(s, &tmp, is_close_round)) return 0;
    if (!get_and_expect_token(s, &tmp, RIGHT_ARROW))                return 0;
    if (!parse_type(s, return_type, 0, 0, 0, 0))              return 0;

    *out = (struct type) {
        .kind = TY_FUNCTION,
        .function_type = (struct function_type) {
            .params = params,
            .return_type = return_type
        },
        .anonymous = !named,
        .name = name.identifier,
        .pointer_count = out->pointer_count
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
        .anonymous = !named,
        .pointer_count = out->pointer_count
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
        .anonymous = !named,
        .pointer_count = out->pointer_count
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
    if (!get_token_type(s, &tmp, IDENTIFIER))                 return 0;
    if (!is_primitive(tmp.identifier, &primitive_type)) {
        seek_back_token(s, 1);
        return 0;
    }

    *out = (struct type) {
        .kind = TY_PRIMITIVE,
        .primitive_type = primitive_type,
        .anonymous = !named,
        .name = name.identifier,
        .pointer_count = out->pointer_count
    };

    return 1;
}

int parse_user_defined_type(struct token_buffer *s,
                            struct type *out)
{
    struct token tmp = {0};
    if (!get_token_type(s, &tmp, IDENTIFIER)) return 0;
    *out = (struct type) {
        .kind = TY_USER_DEFINED,
        .user_defined = tmp.identifier
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
    while (get_token_type(s, &tmp, STAR)) {
        out->pointer_count += 1;
    }

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

    return parse_primitive_type(s, out, named_primitive)
        || parse_user_defined_type(s, out);
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
    LITERAL_NUMERIC,
    LITERAL_NAME,
    LITERAL_HOLE
};

struct literal_expression {
    enum literal_expression_kind kind;
    union {
        int boolean;
        char character;
        struct list_char *str;
        double numeric;
        struct list_char *name;
    };
};

enum binary_operator {
    PLUS_BINARY = 1,
    MINUS_BINARY,
    OR_BINARY,
    AND_BINARY,
    BITWISE_OR_BINARY,
    BITWISE_AND_BINARY,
    GREATER_THAN_BINARY,
    EQUAL_TO_BINARY,
    MULTIPLY_BINARY,
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
    if (get_token_where(s, &tmp, is_keyword_true)) {
        *out = (struct literal_expression) {
            .kind = LITERAL_BOOLEAN,
            .boolean = 1
        };
        return 1;
    }

    if (get_token_where(s, &tmp, is_keyword_false)) {
        *out = (struct literal_expression) {
            .kind = LITERAL_BOOLEAN,
            .boolean = 0
        };
        return 1;
    }

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

int parse_identifier_literal_expression(struct token_buffer *s,
                                  struct literal_expression *out)
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

int parse_literal_expression(struct token_buffer *s,
                             struct literal_expression *out)
{
    return parse_char_literal_expression(s, out)
        || parse_str_literal_expression(s, out)
        || parse_numeric_literal_expression(s, out)
        || parse_boolean_literal_expression(s, out)
        || parse_identifier_literal_expression(s, out);
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

    if (get_token_where(s, &tmp, is_math_minus)) {
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

    if (get_token_where(s, &tmp, is_math_eq)) {
        if (get_token_where(s, &tmp, is_math_eq)) {
            *out = EQUAL_TO_BINARY;
            return 1;
        } else {
            seek_back_token(s, 1);
            return 0;
        }
    }

    return 0;
}

int parse_function_expression(struct token_buffer *s, struct function_expression *out)
{
    struct token tmp = {0};
    struct token name = {0};
    if (!get_token_type(s, &name, IDENTIFIER)) return 0;
    if (!get_token_where(s, &tmp, is_open_round)) {
        seek_back_token(s, 1);
        return 0;
    }

    struct list_expression *params = malloc(sizeof(*params));
    *params = create_list_expression(10);
    int should_continue = 1;

    while (should_continue) {
        struct expression expr = {0};
        if (parse_expression(s, &expr)) {
            append_list_expression(params, expr);
        }
        should_continue = get_token_type(s, &tmp, COMMA);
    }

    if (!get_token_where(s, &tmp, is_close_round)) return 0;

    *out = (struct function_expression) {
        .function_name = name.identifier,
        .params = params
    };

    return 1;
}

int parse_expression_inner(struct token_buffer *s, struct expression *out) {
    struct token tmp = {0};
    enum unary_operator unary_op;

    if (parse_unary_operator(s, &unary_op)) {
        struct expression *nested = malloc(sizeof(*nested));
        if (!parse_expression_inner(s, nested)) return 0;

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

int parse_expression(struct token_buffer *s, struct expression *out) {
    enum binary_operator op;
    int parsed_left = 0;
    struct expression *l = malloc(sizeof(*l));
    struct expression *r = malloc(sizeof(*r));

    if (parse_expression_inner(s, l)) {
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
    RETURN_STATEMENT,
    BLOCK_STATEMENT,
    ACTION_STATEMENT,
    WHILE_LOOP_STATEMENT,
    TYPE_DECLARATION_STATEMENT,
    BREAK_STATEMENT,
    INCLUDE_STATEMENT
};

struct type_declaration_statement {
    struct type type;
    struct list_statement *statements;
};

struct binding_statement {
    struct list_char variable_name;
    struct type variable_type;
    struct expression value;
    int has_type;
};

struct if_statement {
    struct expression condition;
    struct statement *success_statement;
    struct statement *else_statement;
};

struct while_loop_statement {
    struct expression condition;
    struct statement *do_statement;
};

struct include_statement {
    struct list_char include;
    int external;
};

typedef struct statement {
    enum statement_kind kind;
    union {
        struct expression expression;
        struct binding_statement binding_statement;
        struct if_statement if_statement;
        struct list_statement *statements;
        struct while_loop_statement while_loop_statement;
        struct type_declaration_statement type_declaration;
        struct include_statement include_statement;
    };
} statement;

LIST(statement);
CREATE_LIST(statement);
APPEND_LIST(statement);

int parse_statement(struct token_buffer *s, struct statement *out);

int parse_include_statement(struct token_buffer *s, struct statement *out)
{
    int external = 0;
    struct token tmp = {0};
    struct list_char raw_include = {0};
    if (!get_token_type(s, &tmp, HASH))       return 0;
    if (!get_token_type(s, &tmp, IDENTIFIER)) return 0;

    if (get_token_type(s, &tmp, LEFT_ARROW)
        && get_token_type(s, &tmp, IDENTIFIER)) {
        raw_include = *tmp.identifier;
        external = 1;
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

int parse_break_statement(struct token_buffer *s, struct statement *out)
{
    struct token tmp = {0};
    if (!get_token_where(s, &tmp, is_break_keyword)) return 0;
    if (!get_token_type(s, &tmp, SEMICOLON))         return 0;

    *out = (struct statement) {
        .kind = BREAK_STATEMENT
    };

    return 1;
}

int parse_return_statement(struct token_buffer *s, struct statement *out)
{
    struct token tmp = {0};
    struct expression expression = {0};
    if (!get_token_where(s, &tmp, is_return_keyword)) return 0;
    if (!parse_expression(s, &expression))            return 0;
    if (!get_and_expect_token(s, &tmp, SEMICOLON))    return 0;

    *out = (struct statement) {
        .kind = RETURN_STATEMENT,
        .expression = expression
    };

    return 1;
}

int parse_binding_statement(struct token_buffer *s, struct statement *out)
{
    struct token tmp = {0};
    struct type type = {0};
    struct expression expression = {0};
    struct list_char variable_name = {0};
    int has_type = 0;

    if (!get_token_type(s, &tmp, IDENTIFIER)) return 0;
    variable_name = *tmp.identifier;
    if (get_token_type(s, &tmp, COLON)) {
        if (!parse_type(s, &type, 0, 0, 0, 0)) return 0;
        has_type = 1;
    }
    if (!get_and_expect_token_where(s, &tmp, is_math_eq)) {
        seek_back_token(s, 1);
        return 0;
    }
    if (!parse_expression(s, &expression))         return 0;
    if (!get_and_expect_token(s, &tmp, SEMICOLON)) return 0;

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
                          int with_semicolon)
{
    struct token tmp = {0};
    if (!get_token_where(s, &tmp, is_open_curly)) return 0;

    struct list_statement *statements = malloc(sizeof(*statements));
    *statements = create_list_statement(10);

    for (;;) {
        struct statement statement = {0};
        if (parse_statement(s, &statement)) {
            append_list_statement(statements, statement);
        } else {
            return 0;
        }

        if (get_token_where(s, &tmp, is_close_curly)) {
            break;
        }
    }

    if (with_semicolon && !get_token_type(s, &tmp, SEMICOLON)) return 0;

    *out = (struct statement) {
        .kind = BLOCK_STATEMENT,
        .statements = statements
    };

    return 1;
}

int parse_if_statement(struct token_buffer *s, struct statement *out) {
    struct token tmp = {0};
    struct expression condition = {0};
    struct statement *success_statement = malloc(sizeof(*success_statement));
    struct statement *else_statement = NULL;

    if (!get_token_where(s, &tmp, is_keyword_if))             return 0;
    if (!get_and_expect_token_where(s, &tmp, is_open_round))  return 0;
    if (!parse_expression(s, &condition))                     return 0;
    if (!get_and_expect_token_where(s, &tmp, is_close_round)) return 0;
    if (!parse_block_statement(s, success_statement, 0))      return 0;
    if (get_token_where(s, &tmp, is_keyword_else)) {
        else_statement = malloc(sizeof(*else_statement));
        if (!parse_if_statement(s, else_statement) &&
            !parse_block_statement(s, else_statement, 0))
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

int parse_action_statement(struct token_buffer *s, struct statement *out)
{
    struct token tmp = {0};
    struct expression expression = {0};
    if (!parse_expression(s, &expression))         return 0;
    if (!get_and_expect_token(s, &tmp, SEMICOLON)) return 0;

    *out = (struct statement) {
        .kind = ACTION_STATEMENT,
        .expression = expression
    };

    return 1;
}

int parse_while_loop_statement(struct token_buffer *s, struct statement *out)
{
    struct token tmp = {0};
    struct statement *do_statement = malloc(sizeof(*do_statement));
    struct expression expression = {0};

    if (!get_token_where(s, &tmp, is_keyword_while))          return 0;
    if (!get_and_expect_token_where(s, &tmp, is_open_round))  return 0;
    if (!parse_expression(s, &expression))                    return 0;
    if (!get_and_expect_token_where(s, &tmp, is_close_round)) return 0;
    if (!parse_block_statement(s, do_statement, 0))           return 0;

    *out = (struct statement) {
        .kind = WHILE_LOOP_STATEMENT,
        .while_loop_statement = (struct while_loop_statement) {
            .condition = expression,
            .do_statement = do_statement
        }
    };

    return 1;
}

int parse_type_declaration(struct token_buffer *s, struct statement *out) {
    struct type type = {0};
    if (!parse_type(s, &type, 1, 1, 1, 1))   return 0;
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
    if (!parse_block_statement(s, &body, 0)) return 0;

    *out = (struct statement) {
        .kind = TYPE_DECLARATION_STATEMENT,
        .type_declaration = (struct type_declaration_statement) {
            .type = type,
            .statements = body.statements
        }
    };

    return 1;
}

int parse_statement(struct token_buffer *s, struct statement *out) {
    return parse_return_statement(s, out)
        || parse_break_statement(s, out)
        || parse_binding_statement(s, out)
        || parse_if_statement(s, out)
        || parse_block_statement(s, out, 1)
        || parse_action_statement(s, out)
        || parse_while_loop_statement(s, out)
        || parse_type_declaration(s, out)
        || parse_include_statement(s, out);
}

struct rm_file {
    struct list_statement statements;
};

int parse_rm_file(struct token_buffer *s, struct rm_file *out) {
    struct list_statement statements = create_list_statement(10);

    for (;;) {
        struct statement statement = {0};
        if (parse_statement(s, &statement)) {
            append_list_statement(&statements, statement);
        } else {
            break;
        }
    }

    *out = (struct rm_file) {
        .statements = statements
    };

    return 1;
}

// C generation
void write_type(struct type *ty, FILE *file);

void write_primitive_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_PRIMITIVE);
    switch (ty->primitive_type) {
        case UNIT:
            fprintf(file, "void");
            return;
        case I8:
            fprintf(file, "int");
            return;
        case U8:
            fprintf(file, "int");
            return;
        case I16:
            fprintf(file, "int");
            return;
        case U16:
            fprintf(file, "int");
            return;
        case I32:
            fprintf(file, "int");
            return;
        case U32:
            fprintf(file, "int");
            return;
        case I64:
            fprintf(file, "int");
            return;
        case U64:
            fprintf(file, "uint64_t");
            return;
        case USIZE:
            fprintf(file, "size_t");
            return;
        case F32:
            fprintf(file, "float");
            return;
        case F64:
            fprintf(file, "double");
            return;
        case BOOL:
            fprintf(file, "char");
            return;
        default:
            UNREACHABLE("primitive type not handled");
    }
}

void write_struct_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_STRUCT);
    fprintf(file, "struct");
    if (!ty->anonymous) {
        fprintf(file, " %s ", ty->name->data);
    }

    fprintf(file, "{");
    size_t pair_count = ty->key_type_pairs.size;
    for (size_t i = 0; i < pair_count; i++) {
        struct key_type_pair pair = ty->key_type_pairs.data[i];
        write_type(pair.field_type, file);
        fprintf(file, " %s;", pair.field_name.data);
    }
    fprintf(file, "};");
}

void write_enum_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_ENUM);

    size_t variant_count = ty->key_type_pairs.size;
    fprintf(file, "enum %s_kind {", ty->name->data);
    for (size_t i = 0; i < variant_count; i++) {
        fprintf(file, "%s_kind_%s", ty->name->data, ty->key_type_pairs.data[i].field_name.data);
        if (i < variant_count - 1) {
            fprintf(file, ",");
        }
    }

    fprintf(file, "}; ");
    fprintf(file, "struct %s_type { enum %s_kind %s_kind; union {",
            ty->name->data, ty->name->data, ty->name->data);

    for (size_t i = 0; i < variant_count; i++) {
        struct key_type_pair pair = ty->key_type_pairs.data[i];
        write_type(pair.field_type, file);
        fprintf(file, " %s_type_%s;", ty->name->data, pair.field_name.data);
    }
    fprintf(file, "};};");
}

void write_user_defined_type(struct type *ty, FILE *file) {
    // TODO: this is just TMP
    fprintf(file, "struct %s", ty->user_defined->data);
}

void write_function_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_FUNCTION);
    write_type(ty->function_type.return_type, file);
    if (!ty->anonymous) {
        fprintf(file, " %s", ty->name->data);
    }

    fprintf(file, "(");
    size_t param_count = ty->function_type.params.size;
    for (size_t i = 0; i < param_count; i++) {
        struct key_type_pair pair = ty->function_type.params.data[i];
        write_type(pair.field_type, file);
        fprintf(file, " %s", pair.field_name.data);
        if (i < param_count - 1) {
            fprintf(file, ", ");
        }
    }
    fprintf(file, ")");
}

void write_type(struct type *ty, FILE *file) {
    switch (ty->kind) {
        case TY_PRIMITIVE:
            write_primitive_type(ty, file);
            break;
        case TY_STRUCT:
            write_struct_type(ty, file);
            break;
        case TY_FUNCTION:
            write_function_type(ty, file);
            break;
        case TY_ENUM:
            write_enum_type(ty, file);
            break;
        case TY_USER_DEFINED:
            write_user_defined_type(ty, file);
            break;
        default:
            UNREACHABLE("type kind not handled");
    }

    for (int i = 0; i < ty->pointer_count; i++) {
        fprintf(file, "*");
    }
}

void write_expression(struct expression *e, FILE *file);

void write_literal_expression(struct literal_expression *e, FILE *file) {
    switch (e->kind) {
        case LITERAL_BOOLEAN:
        {
            fprintf(file, "%d", e->boolean);
            break;
        }
        case LITERAL_CHAR:
        {
            fprintf(file, "'%c'", e->character);
            break;
        }
        case LITERAL_STR:
        {
            fprintf(file, "\"%s\"", e->str->data);
            break;
        }
        case LITERAL_NUMERIC:
        {
            fprintf(file, "%d", (int)e->numeric); // TODO
            break;
        }
        case LITERAL_NAME:
        {
            fprintf(file, "%s", e->name->data);
            break;
        }
        case LITERAL_HOLE:
            TODO("write hole");
            break;
    }
}

void write_unary_expression(struct unary_expression *e, FILE *file) {
    switch (e->operator) {
        case BANG_UNARY:
            fprintf(file, "!");
            break;
        case STAR_UNARY:
            fprintf(file, "*");
            break;
        case MINUS_UNARY:
            fprintf(file, "-");
            break;
        default:
            UNREACHABLE("unary operator not handled");
    }

    write_expression(e->expression, file);
}

void write_binary_expression(struct binary_expression *e, FILE *file) {
    write_expression(e->l, file);
    switch (e->binary_op) {
        case PLUS_BINARY:
            fprintf(file, " + ");
            break;
        case MINUS_BINARY:
            fprintf(file, " - ");
            break;
        case OR_BINARY:
            fprintf(file, " || ");
            break;
        case AND_BINARY:
            fprintf(file, " && ");
            break;
        case BITWISE_OR_BINARY:
            fprintf(file, " | ");
            break;
        case BITWISE_AND_BINARY:
            fprintf(file, " & ");
            break;
        case GREATER_THAN_BINARY:
            fprintf(file, " > ");
            break;
        case EQUAL_TO_BINARY:
            fprintf(file, " == ");
            break;
        case MULTIPLY_BINARY:
            fprintf(file, " * ");
            break;
        default:
            UNREACHABLE("binary operator not handled");
    }
    write_expression(e->r, file);
}

void write_grouped_expression(struct expression *e, FILE *file) {
    fprintf(file, "(");
    write_expression(e, file);
    fprintf(file, ")");
}

void write_function_expression(struct function_expression *e, FILE *file) {
    fprintf(file, "%s(", e->function_name->data);
    size_t param_count = e->params->size;
    for (size_t i = 0; i < param_count; i++) {
        write_expression(&e->params->data[i], file);
        if (i < param_count - 1) {
            fprintf(file, ", ");
        }
    }
    fprintf(file, ")");
}

void write_expression(struct expression *e, FILE *file) {
    switch (e->kind) {
        case LITERAL_EXPRESSION:
            write_literal_expression(&e->literal, file);
            return;
        case UNARY_EXPRESSION:
            write_unary_expression(&e->unary, file);
            return;
        case BINARY_EXPRESSION:
            write_binary_expression(&e->binary, file);
            return;
        case GROUP_EXPRESSION:
            write_grouped_expression(e->grouped, file);
            return;
        case FUNCTION_EXPRESSION:
            write_function_expression(&e->function, file);
            return;
        default:
            UNREACHABLE("expression kind not handled");
    }
}

void write_statement(struct statement *s, FILE *file);

void write_binding_statement(struct binding_statement *s, FILE *file) {
    if (*&s->has_type) {
        write_type(&s->variable_type, file);
    }
    fprintf(file, " %s = ", s->variable_name.data);
    write_expression(&s->value, file);
    fprintf(file, ";");
}

void write_if_statement(struct if_statement *s, FILE *file) {
    fprintf(file, "if (");
    write_expression(&s->condition, file);
    fprintf(file, ")");
    write_statement(s->success_statement, file);
    if (s->else_statement != NULL) {
        fprintf(file, " else ");
        write_statement(s->else_statement, file);
    }
}

void write_return_statement(struct expression *e, FILE *file) {
    fprintf(file, "return ");
    write_expression(e, file);
    fprintf(file, ";");
}

void write_block_statement(struct list_statement *statements, FILE *file) {
    fprintf(file, "{");
    for (size_t i = 0; i < statements->size; i++) {
        write_statement(&statements->data[i], file);
    }
    fprintf(file, "}");
}

void write_action_statement(struct expression *e, FILE *file) {
    write_expression(e, file);
    fprintf(file, ";");
}

void write_while_statement(struct while_loop_statement *s, FILE *file) {
    fprintf(file, "while (");
    write_expression(&s->condition, file);
    fprintf(file, ")");
    write_statement(s->do_statement, file);
}

void write_type_declaration_statement(struct type_declaration_statement *s, FILE *file) {
    write_type(&s->type, file);
    if (s->statements != NULL)
    {
        fprintf(file, "{");
        for (size_t i = 0; i < s->statements->size; i++) {
            write_statement(&s->statements->data[i], file);
        }
        fprintf(file, "}");
    }
}

void write_break_statement(FILE *file) {
    fprintf(file, "break;");
}

void write_include_statement(struct include_statement *s, FILE *file) {
    fprintf(file, "#include");
    if (s->external) {
        fprintf(file, " <");
    } else {
        fprintf(file, " \"");
    }
    fprintf(file, "%s", s->include.data);
    if (s->external) {
        fprintf(file, ">");
    } else {
        fprintf(file, "\"");
    }
    fprintf(file, "\n");
}

void write_statement(struct statement *s, FILE *file) {
    switch (s->kind) {
        case BINDING_STATEMENT:
            write_binding_statement(&s->binding_statement, file);
            break;
        case IF_STATEMENT:
            write_if_statement(&s->if_statement, file);
            break;
        case RETURN_STATEMENT:
            write_return_statement(&s->expression, file);
            break;
        case BLOCK_STATEMENT:
            write_block_statement(s->statements, file);
            break;
        case ACTION_STATEMENT:
            write_action_statement(&s->expression, file);
            break;
        case WHILE_LOOP_STATEMENT:
            write_while_statement(&s->while_loop_statement, file);
            break;
        case TYPE_DECLARATION_STATEMENT:
            write_type_declaration_statement(&s->type_declaration, file);
            break;
        case BREAK_STATEMENT:
            write_break_statement(file);
            break;
        case INCLUDE_STATEMENT:
            write_include_statement(&s->include_statement, file);
            break;
        default:
            UNREACHABLE("statement type not handled");
    }
}
