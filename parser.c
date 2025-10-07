#include "list.c"
#include <assert.h>
#include <math.h>
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

struct_list(positional_char);
struct_list(char);

struct file_buffer {
    struct positional_char *data;
    size_t current_position;
    size_t size;
};

struct file_buffer create_file_buffer(FILE *fstream) {
    #define tmp_buf_size 1024
    static char buffer[tmp_buf_size];
    struct list_positional_char chars = list_create(positional_char, tmp_buf_size);

    int row = 1;
    int col = 1;
    size_t read_amount = 0;

    while ((read_amount = fread(&buffer, sizeof(char), tmp_buf_size, fstream)) > 0)
    {
        for (size_t i = 0; i < read_amount; i++) {
            struct positional_char pc = (struct positional_char) {
                .value = buffer[i],
                .row = row,
                .col = col
            };

            list_append(&chars, pc);

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

        list_append(out, test.value);
    }
}

void copy_list_char(struct list_char *dest, struct list_char *src) {
    for (size_t i = 0; i < src->size && src->data[i] != '\0'; i++) {
        list_append(dest, src->data[i]);
    }
}

void append_list_char_slice(struct list_char *dest, char *slice) {
    while (*slice != '\0') {
        list_append(dest, *slice);
        slice++;
    }
}

int list_char_eq(struct list_char *l, struct list_char *r) {
	if (l->size != r->size) {
		return 0;
	}
	
	for (size_t i = 0; i < l->size; i++) {
		if (l->data[i] != r->data[i]) {
			return 0;
		}
	}

	return 1;
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
    HASH,
    DOT,
    QUESTION_MARK
};

enum keyword_type {
    FN_KEYWORD = 1,
    ENUM_KEYWORD,
    STRUCT_KEYWORD,
    IF_KEYWORD,
    WHILE_KEYWORD,
    RETURN_KEYWORD,
    BOOLEAN_TRUE_KEYWORD,
    BOOLEAN_FALSE_KEYWORD,
    ELSE_KEYWORD,
    BREAK_KEYWORD,
    MUTABLE_KEYWORD,
    NULL_KEYWORD,
    SWITCH_KEYWORD,
    CASE_KEYWORD
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

struct_list(token);

int is_keyword(struct list_char *ident, enum keyword_type *out) {
    if (strcmp(ident->data, "fn") == 0) {
        *out = FN_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "enum") == 0) {
        *out = ENUM_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "struct") == 0) {
        *out = STRUCT_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "if") == 0) {
        *out = IF_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "while") == 0) {
        *out = WHILE_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "return") == 0) {
        *out = RETURN_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "break") == 0) {
        *out = BREAK_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "true") == 0) {
        *out = BOOLEAN_TRUE_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "false") == 0) {
        *out = BOOLEAN_FALSE_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "else") == 0) {
        *out = ELSE_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "mut") == 0) {
        *out = MUTABLE_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "null") == 0) {
        *out = NULL_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "switch") == 0) {
        *out = SWITCH_KEYWORD;
        return 1;
    }

    if (strcmp(ident->data, "case") == 0) {
        *out = CASE_KEYWORD;
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
        case '.':
        case '?':
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
            case '.': {
                *out = (struct token) {
                    .token_type = DOT,
                    .position = b->current_position
                };
                return 1;
            }
            case '?': {
                *out = (struct token) {
                    .token_type = QUESTION_MARK,
                    .position = b->current_position
                };
                return 1;
            }
            case '\'': {
                int char_start_position = b->current_position;
                struct list_char *c = malloc(sizeof(*c));
                *c = list_create(char, 2);
                if (!read_file_buffer(b, 1, &test)) return 0;
                list_append(c, test.value);
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
                *str = list_create(char, 50);
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
                *ident = list_create(char, 10);
                list_append(ident, test.value);
                read_until(b, ident, is_special_or_whitespace, 0);
                list_append(ident, '\0');

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
    struct list_token tokens = list_create(token, b->size);
    struct token tok;

    while (next_token(b, &tok)) {
        list_append(&tokens, tok);
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
    return t->token_type == KEYWORD && t->keyword_type == RETURN_KEYWORD;
}

int is_break_keyword(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == BREAK_KEYWORD;
}

int is_keyword_true(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == BOOLEAN_TRUE_KEYWORD;
}

int is_keyword_false(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == BOOLEAN_FALSE_KEYWORD;
}

int is_keyword_if(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == IF_KEYWORD;
}

int is_keyword_else(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == ELSE_KEYWORD;
}

int is_keyword_while(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == WHILE_KEYWORD;
}

int is_keyword_mut(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == MUTABLE_KEYWORD;
}

int is_keyword_struct(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == STRUCT_KEYWORD;
}

int is_keyword_enum(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == ENUM_KEYWORD;
}

int is_keyword_null(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == NULL_KEYWORD;
}

int is_keyword_switch(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == SWITCH_KEYWORD;
}

int is_keyword_case(struct token *t) {
    return t->token_type == KEYWORD && t->keyword_type == CASE_KEYWORD;
}

// parsing
#define parser_t int (*)(struct token_buffer *, void *)
int try_parse(struct token_buffer *s,
              void *out,
              int (*parser)(struct token_buffer *, void *))
{
    size_t start = s->current_position;
    if (!parser(s, out)) {
        seek_back_token(s, s->current_position - start);
        return 0;
    }
    
    return 1;
}

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

enum type_modifier_kind {
    POINTER_MODIFIER_KIND = 1,
    NULLABLE_MODIFIER_KIND,
    ARRAY_MODIFIER_KIND,
    MUTABLE_MODIFIER_KIND
};

struct array_type_modifier {
    int sized;
    int size;
};

typedef struct type_modifier {
    enum type_modifier_kind kind;
    union {
        struct array_type_modifier array_modifier;
    };
} type_modifier;

struct_list(type_modifier);

enum type_kind {
    TY_PRIMITIVE = 1,
    TY_STRUCT,
    TY_FUNCTION,
    TY_ENUM
};

typedef struct key_type_pair {
    struct list_char field_name;
    struct type *field_type;
} key_type_pair;

struct_list(key_type_pair);

struct function_type {
    struct list_key_type_pair params;
    struct type *return_type;
};
    
struct struct_type {
    struct list_key_type_pair pairs;
    int predefined;
};

struct enum_type {
    struct list_key_type_pair pairs;
    int predefined;
};

typedef struct type {
    enum type_kind kind;
    struct list_char *name;
    struct list_type_modifier modifiers;
    union {
        struct function_type function_type;
        enum primitive_type primitive_type;
        struct struct_type struct_type;
        struct enum_type enum_type;
    };
} type;

struct key_type_pair create_key_type_pair() {
    return (struct key_type_pair) {
        .field_name = list_create(char, 10),
        .field_type = malloc(sizeof(struct type))
    };
}

struct_list(type);

int parse_pointer_type_modifier(struct token_buffer *tb, struct type_modifier *out)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, STAR)) return 0;
    *out = (struct type_modifier) {
        .kind = POINTER_MODIFIER_KIND,
    };

    return 1;
}

int parse_nullable_type_modifier(struct token_buffer *tb, struct type_modifier *out)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, QUESTION_MARK)) return 0;
    *out = (struct type_modifier) {
        .kind = NULLABLE_MODIFIER_KIND,
    };

    return 1;
}

int parse_array_type_modifier(struct token_buffer *tb, struct type_modifier *out)
{
    struct token tmp = {0};
    int size = 0;
    int sized = 0;
    if (!get_token_where(tb, &tmp, is_open_square)) return 0;
    if (get_token_type(tb, &tmp, NUMERIC)) {
        sized = 1;
        size = (int)tmp.numeric; // TODO: numeric token needs improving
    }
    if (!get_token_where(tb, &tmp, is_close_square)) return 0;
    *out = (struct type_modifier) {
        .kind = ARRAY_MODIFIER_KIND,
        .array_modifier = (struct array_type_modifier) {
            .size = size,
            .sized = sized
        }
    };

    return 1;
}

int parse_mutable_type_modifier(struct token_buffer *tb, struct type_modifier *out)
{
    struct token tmp = {0};
    if (!get_token_where(tb, &tmp, is_keyword_mut)) return 0;
    *out = (struct type_modifier) {
        .kind = MUTABLE_MODIFIER_KIND,
    };

    return 1;
}

int parse_type_modifier(struct token_buffer *tb, struct type_modifier *out)
{
    return try_parse(tb, out, (parser_t)parse_pointer_type_modifier)
        || try_parse(tb, out, (parser_t)parse_nullable_type_modifier)
        || try_parse(tb, out, (parser_t)parse_array_type_modifier)
        || try_parse(tb, out, (parser_t)parse_mutable_type_modifier);
}

struct list_type_modifier parse_modifiers(struct token_buffer *tb)
{
    struct list_type_modifier modifiers = list_create(type_modifier, 5);
    struct type_modifier modifier = {0};
    while (parse_type_modifier(tb, &modifier)) {
        list_append(&modifiers, modifier);
    }
    return modifiers;
}

int parse_type(struct token_buffer *s,
               struct type *out,
               int named_fn,
               int predefined);

int parse_key_type_pairs(struct token_buffer *s,
                         struct list_key_type_pair *out)
{
    struct token tmp = {0};
    int should_continue = 1;

    while (should_continue) {
        struct key_type_pair pair = create_key_type_pair();
        if (!get_token_type(s, &tmp, IDENTIFIER))  return 0;
        pair.field_name = *tmp.identifier;
        if (!get_token_type(s, &tmp, COLON))       return 0;
        if (!parse_type(s, pair.field_type, 0, 1)) return 0;
        list_append(out, pair);
        should_continue = get_token_type(s, &tmp, COMMA);
    }

    return 1;
}

int parse_function_type(struct token_buffer *s, struct type *out, int named)
{
    struct token tmp;
    struct token name;
    struct list_key_type_pair params = list_create(key_type_pair, 10);
    struct type *return_type = malloc(sizeof(*return_type));

    if (named && !get_and_expect_token(s, &name, IDENTIFIER)) return 0;
    if (!get_and_expect_token_where(s, &tmp, is_open_round))  return 0;
    parse_key_type_pairs(s, &params);
    if (!get_and_expect_token_where(s, &tmp, is_close_round)) return 0;
    if (!get_and_expect_token(s, &tmp, RIGHT_ARROW))          return 0;
    if (!parse_type(s, return_type, 0, 1))                    return 0;

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

int parse_struct_type(struct token_buffer *s, struct type *out, int predefined_type)
{
    struct token name = {0};
    struct token tmp = {0};
    struct list_key_type_pair pairs = list_create(key_type_pair, 10);

    if (!get_and_expect_token(s, &name, IDENTIFIER))              return 0;
    if (!predefined_type && get_token_where(s, &tmp, is_open_curly)) {
        if (!parse_key_type_pairs(s, &pairs))                     return 0;
        if (!get_and_expect_token_where(s, &tmp, is_close_curly)) return 0;
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

int parse_enum_type(struct token_buffer *s, struct type *out, int predefined_type)
{
    struct token name = {0};
    struct token tmp = {0};
    struct list_key_type_pair pairs = list_create(key_type_pair, 10);

    if (!get_and_expect_token(s, &name, IDENTIFIER))              return 0;
    if (!predefined_type && get_token_where(s, &tmp, is_open_curly)) {
        if (!parse_key_type_pairs(s, &pairs))                     return 0;
        if (!get_and_expect_token_where(s, &tmp, is_close_curly)) return 0;
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

int parse_primitive_type(struct token_buffer *s, struct type *out)
{
    struct token tmp = {0};
    enum primitive_type primitive_type;
    struct token name = {0};

    if (!get_token_type(s, &tmp, IDENTIFIER))                 return 0;
    if (!is_primitive(tmp.identifier, &primitive_type))       return 0;

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
               int predefined)
{
    struct token tmp = {0};
    out->modifiers = parse_modifiers(s);

    if (get_token_type(s, &tmp, KEYWORD)) {
        switch (tmp.keyword_type) {
            case FN_KEYWORD:
                return parse_function_type(s, out, named_fn);
            case ENUM_KEYWORD:
                return parse_enum_type(s, out, predefined);
            case STRUCT_KEYWORD:
                return parse_struct_type(s, out, predefined);
            default:
                seek_back_token(s, 1);
                return 0;
        }
    }

    return try_parse(s, out, (parser_t)parse_primitive_type);
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
    LITERAL_HOLE,
    LITERAL_STRUCT,
    LITERAL_ENUM,
    LITERAL_NULL
};

typedef struct key_expression {
    struct list_char *key;
    struct expression *expression;
} key_expression;

struct_list(key_expression);
    
struct literal_struct_enum {
    struct list_char *name;
    struct list_key_expression key_expr_pairs;
};

struct literal_expression {
    enum literal_expression_kind kind;
    union {
        int boolean;
        char character;
        struct list_char *str;
        double numeric;
        struct list_char *name;
        struct literal_struct_enum struct_enum;
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
    LESS_THAN_BINARY,
    EQUAL_TO_BINARY,
    MULTIPLY_BINARY,
    DOT_BINARY
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

struct_list(expression);

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

int parse_struct_enum_literal_expression(struct token_buffer *s,
                                         struct literal_expression *out)
{
    struct token tmp = {0};
    struct token name = {0};
    enum literal_expression_kind kind = 0;

    if (get_token_where(s, &tmp, is_keyword_struct)) {
        kind = LITERAL_STRUCT;
    } else if (get_token_where(s, &tmp, is_keyword_enum)) {
        kind = LITERAL_ENUM;
    } else {
        return 0;
    }

    if (!get_token_type(s, &name, IDENTIFIER))    return 0;
    if (!get_token_where(s, &tmp, is_open_curly)) return 0;

    struct list_key_expression pairs = list_create(key_expression, 10);
    int should_continue = 1;
    while (should_continue) {
        struct key_expression pair = {0};
        if (!get_token_type(s, &tmp, IDENTIFIER))  return 0;
        pair.key = tmp.identifier;
        if (!get_token_where(s, &tmp, is_math_eq)) return 0;
        struct expression *e = malloc(sizeof(*e));
        if (!parse_expression(s, e))               return 0;
        pair.expression = e;
        list_append(&pairs, pair);
        should_continue = get_token_type(s, &tmp, COMMA);
    }
    
    if (!get_token_where(s, &tmp, is_close_curly)) return 0;

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
                                  struct literal_expression *out)
{
    struct token tmp = {0};
    if (!get_token_where(s, &tmp, is_keyword_null)) return 0;

    *out = (struct literal_expression) {
        .kind = LITERAL_NULL,
    };

    return 1;
}

int parse_literal_expression(struct token_buffer *s,
                             struct literal_expression *out)
{
    return try_parse(s, out, (parser_t)parse_char_literal_expression)
        || try_parse(s, out, (parser_t)parse_str_literal_expression)
        || try_parse(s, out, (parser_t)parse_numeric_literal_expression)
        || try_parse(s, out, (parser_t)parse_boolean_literal_expression)
        || try_parse(s, out, (parser_t)parse_identifier_literal_expression)
        || try_parse(s, out, (parser_t)parse_struct_enum_literal_expression)
        || try_parse(s, out, (parser_t)parse_null_literal_expression);
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

    if (get_token_type(s, &tmp, LEFT_ARROW)) {
        *out = LESS_THAN_BINARY;
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

    if (get_token_type(s, &tmp, DOT)) {
        *out = DOT_BINARY;
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
    *params = list_create(expression, 10);
    int should_continue = 1;

    while (should_continue) {
        struct expression expr = {0};
        if (parse_expression(s, &expr)) {
            list_append(params, expr);
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
    if (try_parse(s, &function, (parser_t)parse_function_expression)) {
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
    INCLUDE_STATEMENT,
	SWITCH_STATEMENT
};

enum switch_pattern_kind {
    OBJECT_PATTERN_KIND,
    ARRAY_PATTERN_KIND,
    NUMBER_PATTERN_KIND,
    STRING_PATTERN_KIND,
    VARIABLE_PATTERN_KIND,
    REST_PATTERN_KIND
};
    
typedef struct key_pattern_pair {
    struct list_char key;
    struct switch_pattern *pattern;
} key_pattern_pair;

struct_list(key_pattern_pair);

struct object_pattern {
    struct list_key_pattern_pair pairs;
};

struct array_pattern {
    struct list_switch_pattern *patterns;
};

struct number_pattern {
    double number;
};

struct string_pattern {
    struct list_char str;
};

struct variable_pattern {
    struct list_char variable_name;
};

typedef struct switch_pattern {
    enum switch_pattern_kind switch_pattern_kind;
    union {
        struct object_pattern object_pattern;
        struct array_pattern array_pattern;
        struct number_pattern number_pattern;
        struct string_pattern string_pattern;
        struct variable_pattern variable_pattern;
    };
} switch_pattern;

struct_list(switch_pattern);

int parse_switch_pattern(struct token_buffer *tb, struct switch_pattern *out);

int parse_variable_pattern(struct token_buffer *tb, struct switch_pattern *out)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, IDENTIFIER)) return 0;
    *out = (struct switch_pattern) {
        .switch_pattern_kind = VARIABLE_PATTERN_KIND,
        .variable_pattern = (struct variable_pattern) {
            .variable_name = *tmp.identifier
        }
    };

    return 1;
}

int parse_string_pattern(struct token_buffer *tb, struct switch_pattern *out)
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

int parse_number_pattern(struct token_buffer *tb, struct switch_pattern *out)
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

int parse_rest_pattern(struct token_buffer *tb, struct switch_pattern *out)
{
    struct token tmp = {0};
    if (!get_token_type(tb, &tmp, DOT)) return 0;
    if (!get_token_type(tb, &tmp, DOT)) return 0;

    *out = (struct switch_pattern) {
        .switch_pattern_kind = REST_PATTERN_KIND
    };
    
    return 1;
}

int parse_array_pattern(struct token_buffer *tb, struct switch_pattern *out)
{
    struct token tmp = {0};
    struct list_switch_pattern *patterns = malloc(sizeof(*patterns));
    *patterns = list_create(switch_pattern, 10);
    int should_continue = 1;

    if (!get_token_where(tb, &tmp, is_open_square)) return 0;
    while (should_continue) {
        struct switch_pattern p = {0};
        if (parse_switch_pattern(tb, &p)) {
            list_append(patterns, p);
        }
        should_continue = get_token_type(tb, &tmp, COMMA);
    }
    if (!get_token_where(tb, &tmp, is_close_square)) return 0;
    *out = (switch_pattern) {
        .switch_pattern_kind = ARRAY_PATTERN_KIND,
        .array_pattern = (struct array_pattern) {
            .patterns = patterns
        }
    };

    return 1;
}

int parse_key_pattern_pair(struct token_buffer *tb, struct key_pattern_pair *out)
{
    struct token tmp = {0};
    struct list_char key = {0};
    struct switch_pattern *pattern = malloc(sizeof(*pattern));
    if (!get_token_type(tb, &tmp, IDENTIFIER)) {
        if (!parse_rest_pattern(tb, pattern)) return 0;
        *out = (struct key_pattern_pair) {
            .key = key,
            .pattern = pattern
        };
        return 1;
    }
    key = *tmp.identifier;
    if (!get_token_type(tb, &tmp, COLON))      return 0;
    if (parse_switch_pattern(tb, pattern))     return 0;
    *out = (struct key_pattern_pair) {
        .key = key,
        .pattern = pattern
    };

    return 0;
}

int parse_object_pattern(struct token_buffer *tb, struct switch_pattern *out)
{
    struct token tmp = {0};
    int should_continue = 1;
    struct list_key_pattern_pair pairs = list_create(key_pattern_pair, 10);

    if (!get_token_where(tb, &tmp, is_open_curly)) return 0;
    while (should_continue) {
        struct key_pattern_pair p = {0};
        if (parse_key_pattern_pair(tb, &p)) {
            list_append(&pairs, p);
        }
        should_continue = get_token_type(tb, &tmp, COMMA);
    }
    if (!get_token_where(tb, &tmp, is_close_curly)) return 0;
    *out = (struct switch_pattern) {
        .switch_pattern_kind = OBJECT_PATTERN_KIND,
        .object_pattern = (struct object_pattern) {
            .pairs = pairs
        }
    };

    return 1;
}

int parse_switch_pattern(struct token_buffer *tb, struct switch_pattern *out)
{
    return try_parse(tb, out, (parser_t)parse_object_pattern)
        || try_parse(tb, out, (parser_t)parse_array_pattern)
        || try_parse(tb, out, (parser_t)parse_rest_pattern)
        || try_parse(tb, out, (parser_t)parse_number_pattern)
        || try_parse(tb, out, (parser_t)parse_string_pattern)
        || try_parse(tb, out, (parser_t)parse_variable_pattern);
}

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

typedef struct case_statement {
	struct switch_pattern pattern;
    struct statement *statement;
} case_statement;

struct_list(case_statement);

struct switch_statement {
	struct expression switch_expression;
	struct list_case_statement cases;
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
        struct switch_statement switch_statement;
    };
} statement;

struct_list(statement);

int parse_statement(struct token_buffer *s, struct statement *out);

int parse_include_statement(struct token_buffer *s, struct statement *out)
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
    
    if (!get_token_type(s, &tmp, IDENTIFIER))             return 0;
    variable_name = *tmp.identifier;
    if (get_token_type(s, &tmp, COLON)) {
        if (!parse_type(s, &type, 0, 1))                  return 0;
        has_type = 1;
    }
    if (!get_and_expect_token_where(s, &tmp, is_math_eq)) return 0;
    if (!parse_expression(s, &expression))                return 0;
    if (!get_and_expect_token(s, &tmp, SEMICOLON))        return 0;

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
                          struct statement *out)
{
    struct token tmp = {0};
    if (!get_token_where(s, &tmp, is_open_curly)) return 0;

    struct list_statement *statements = malloc(sizeof(*statements));
    *statements = list_create(statement, 10);

    for (;;) {
        struct statement statement = {0};
        if (parse_statement(s, &statement)) {
            list_append(statements, statement);
        } else {
            return 0;
        }

        if (get_token_where(s, &tmp, is_close_curly)) {
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
                       struct statement *out)
{
    struct token tmp = {0};
    struct expression condition = {0};
    struct statement *success_statement = malloc(sizeof(*success_statement));
    struct statement *else_statement = NULL;

    if (!get_token_where(s, &tmp, is_keyword_if))             return 0;
    if (!get_and_expect_token_where(s, &tmp, is_open_round))  return 0;
    if (!parse_expression(s, &condition))                     return 0;
    if (!get_and_expect_token_where(s, &tmp, is_close_round)) return 0;
    if (!parse_block_statement(s, success_statement))         return 0;
    if (get_token_where(s, &tmp, is_keyword_else)) {
        else_statement = malloc(sizeof(*else_statement));
        if (!parse_if_statement(s, else_statement) &&
            !parse_block_statement(s, else_statement))
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
    if (!parse_block_statement(s, do_statement))              return 0;

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
    
    if (!parse_type(s, &type, 1, 0)) return 0;
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
    if (!parse_block_statement(s, &body)) return 0;

    *out = (struct statement) {
        .kind = TYPE_DECLARATION_STATEMENT,
        .type_declaration = (struct type_declaration_statement) {
            .type = type,
            .statements = body.statements
        }
    };

    return 1;
}

int parse_case_statement(struct token_buffer *tb, struct case_statement *out)
{
    struct token tmp = {0};
    struct switch_pattern pattern = {0};
    struct statement *s = malloc(sizeof(*s));
    if (!get_token_where(tb, &tmp, is_keyword_case)) return 0;
    if (!parse_switch_pattern(tb, &pattern))         return 0;
    if (!get_token_type(tb, &tmp, COLON))            return 0;
    if (!parse_statement(tb, s))                     return 0;
    *out = (struct case_statement) {
        .pattern = pattern,
        .statement = s
    };
    
    return 1;
}

int parse_switch_statement(struct token_buffer *tb, struct statement *out)
{
    struct token tmp = {0};
    struct expression switch_on = {0};
    struct list_case_statement cases = list_create(case_statement, 10);
    int should_continue = 1;

    if (!get_token_where(tb, &tmp, is_keyword_switch)) return 0;
    if (!get_token_where(tb, &tmp, is_open_round))     return 0;
    if (!parse_expression(tb, &switch_on))             return 0;
    if (!get_token_where(tb, &tmp, is_close_round))    return 0;
    if (!get_token_where(tb, &tmp, is_open_curly))     return 0;

    while (should_continue) {
        struct case_statement case_s = {0};
        if (!parse_case_statement(tb, &case_s))        return 0;
        list_append(&cases, case_s);
        should_continue = !get_token_where(tb, &tmp, is_close_curly);
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

int parse_statement(struct token_buffer *s, struct statement *out) {
    return try_parse(s, out, (parser_t)parse_return_statement)
        || try_parse(s, out, (parser_t)parse_break_statement)
        || try_parse(s, out, (parser_t)parse_binding_statement)
        || try_parse(s, out, (parser_t)parse_if_statement)
        || try_parse(s, out, (parser_t)parse_block_statement)
        || try_parse(s, out, (parser_t)parse_action_statement)
        || try_parse(s, out, (parser_t)parse_while_loop_statement)
        || try_parse(s, out, (parser_t)parse_type_declaration)
        || try_parse(s, out, (parser_t)parse_include_statement)
        || try_parse(s, out, (parser_t)parse_switch_statement);
}

struct rm_file {
    struct list_statement statements;
};

int parse_rm_file(struct token_buffer *s, struct rm_file *out) {
    struct list_statement statements = list_create(statement, 10);

    for (;;) {
        struct statement statement = {0};
        if (parse_statement(s, &statement)) {
            list_append(&statements, statement);
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
struct statement_slice {
	struct statement *statements;
	size_t len;
};

struct c_scope {
	struct statement_slice preceding_statements;
    struct list_key_type_pair scoped_variables;
	struct c_scope *parent;
};

void write_type(struct type *ty, FILE *file);

void write_primitive_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_PRIMITIVE);
    switch (ty->primitive_type) {
        case UNIT:
            fprintf(file, "void");
            return;
        case I8:
            fprintf(file, "char");
            return;
        case U8:
            fprintf(file, "unsigned char");
            return;
        case I16:
            fprintf(file, "int");
            return;
        case U16:
            fprintf(file, "unsigned int");
            return;
        case I32:
            fprintf(file, "int");
            return;
        case U32:
            fprintf(file, "unsigned int");
            return;
        case I64:
            fprintf(file, "long");
            return;
        case U64:
            fprintf(file, "unsigned long");
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

void append_int(int input, struct list_char *out) {
    size_t size = (int)(ceil(log10(input)) + 1);
    char *str = malloc(size);
    sprintf(str, "%d", input);
    for (size_t i = 0; i < size && str[i] != '\0'; i++) {
        list_append(out, str[i]);
    }
    free(str);
}

struct list_char apply_type_modifier(struct type_modifier modifier, struct list_char input)
{
    struct list_char output = list_create(char, input.size * 2);
    switch (modifier.kind) {
        case POINTER_MODIFIER_KIND:
        {
            list_append(&output, '(');
            list_append(&output, '*');
            copy_list_char(&output, &input);
            list_append(&output, ')');
            break;
        }
        case NULLABLE_MODIFIER_KIND:
        {
            copy_list_char(&output, &input);
            break;
        }
        case ARRAY_MODIFIER_KIND:
        {
            list_append(&output, '(');
            copy_list_char(&output, &input);
            list_append(&output, '[');
            if (modifier.array_modifier.sized) {
                append_int(modifier.array_modifier.size, &output);
            }
            list_append(&output, ']');
            list_append(&output, ')');
            break;
        }
        case MUTABLE_MODIFIER_KIND:
            // TODO: this makes it tricky. Revisit.
            break;
        }
    list_append(&output, '\0');
    return output;
}

struct list_char apply_type_modifiers(struct list_type_modifier modifiers, struct list_char input)
{
    struct list_char output = input;
    for (size_t i = 0; i < modifiers.size; i++) {
        output = apply_type_modifier(modifiers.data[i], output);
    }
    return output;
}

void write_struct_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_STRUCT);
    if (ty->struct_type.predefined) {
        fprintf(file, "struct %s", ty->name->data);
        return;
    }

    fprintf(file, "struct %s {", ty->name->data);
    size_t pair_count = ty->struct_type.pairs.size;
    for (size_t i = 0; i < pair_count; i++) {
        struct key_type_pair pair = ty->struct_type.pairs.data[i];
        write_type(pair.field_type, file);
        struct list_char modified = apply_type_modifiers(pair.field_type->modifiers, pair.field_name);
        fprintf(file, " %s", modified.data);
        fprintf(file, ";");
    }
    fprintf(file, "};");
}

void write_enum_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_ENUM);
    if (ty->enum_type.predefined) {
        fprintf(file, "struct %s_type", ty->name->data);
        return;
    }

    size_t variant_count = ty->enum_type.pairs.size;
    fprintf(file, "enum %s_kind {", ty->name->data);
    for (size_t i = 0; i < variant_count; i++) {
        fprintf(file, "%s_kind_%s", ty->name->data, ty->enum_type.pairs.data[i].field_name.data);
        if (i < variant_count - 1) {
            fprintf(file, ",");
        }
    }

    fprintf(file, "}; ");
    fprintf(file, "struct %s_type { enum %s_kind %s_kind; union {",
            ty->name->data, ty->name->data, ty->name->data);

    for (size_t i = 0; i < variant_count; i++) {
        struct key_type_pair pair = ty->enum_type.pairs.data[i];
        write_type(pair.field_type, file);
        struct list_char union_name = list_create(char, pair.field_name.size * 2);
        for (size_t j = 0; j < ty->name->size; j++) {
            char to_add = ty->name->data[j];
            if (to_add != '\0') {
                list_append(&union_name, ty->name->data[j]);
            }
        } 
        append_list_char_slice(&union_name, "_type_");
        for (size_t j = 0; j < pair.field_name.size; j++) {
            list_append(&union_name, pair.field_name.data[j]);
        } 
        struct list_char modified = apply_type_modifiers(pair.field_type->modifiers, union_name);
        fprintf(file, " %s;", modified.data);
    }
    fprintf(file, "};};");
    
    // TODO: current idea is to generate constructor functions per enum variant
    // 
    // struct person { int age; };
    // enum result_kind {
    //     result_kind_ok,
    //     result_kind_error
    // };
    // struct result_type {
    //     enum result_kind result_kind;
    //     union {
    //      struct person result_type_ok;
    //      char result_type_error;
    //     };
    // };
    //
    // would produce the following:
    // struct result_type result_type_ok(struct person p);
    // struct result_type result_type_error(char err);
    // Then when we write the binding expressions for enums we can map to these functions
}

void write_function_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_FUNCTION);
    write_type(ty->function_type.return_type, file);
    struct list_type_modifier return_modifiers = ty->function_type.return_type->modifiers;
    for (size_t i = 0; i < return_modifiers.size; i++) {
        if (return_modifiers.data[i].kind == POINTER_MODIFIER_KIND) {
            fprintf(file, "*");
        }
    }
    fprintf(file, " %s(", ty->name->data);

    size_t param_count = ty->function_type.params.size;
    for (size_t i = 0; i < param_count; i++) {
        struct key_type_pair pair = ty->function_type.params.data[i];
        write_type(pair.field_type, file);
        struct list_char modified = apply_type_modifiers(pair.field_type->modifiers, pair.field_name);
        fprintf(file, " %s", modified.data);
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
        default:
            UNREACHABLE("type kind not handled");
    }
}

void write_expression(struct expression *e, struct c_scope *scope, FILE *file);

void write_literal_expression(struct literal_expression *e, struct c_scope *scope, FILE *file) {
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
        case LITERAL_STRUCT:
        {
            fprintf(file, "(struct %s) {", e->struct_enum.name->data);
            size_t pair_count = e->struct_enum.key_expr_pairs.size;
            for (size_t i = 0; i < pair_count; i++) {
                struct key_expression pair = e->struct_enum.key_expr_pairs.data[i];
                fprintf(file, ".%s = ", pair.key->data);
                write_expression(pair.expression, scope, file);
                if (i + 1 < pair_count) {
                    fprintf(file, ",");
                }
            }
            fprintf(file, "}");
            break;
        }
        case LITERAL_ENUM:
            TODO("literal enum");
            break;
        case LITERAL_NULL:
            // TODO: this is tmp
            fprintf(file, "NULL");
            break;
        }
}

int get_scoped_variable_type(struct c_scope *scope,
					         struct list_char variable_name,
							 struct type *out)
{
	for (size_t i = 0; i < scope->preceding_statements.len; i++) {
		struct statement s = scope->preceding_statements.statements[i];
		if (s.kind == BINDING_STATEMENT) {
			if (list_char_eq(&s.binding_statement.variable_name, &variable_name)
				&& s.binding_statement.has_type)
			{
				*out = s.binding_statement.variable_type;
				return 1;
			}
		}
	}

    for (size_t i = 0; i < scope->scoped_variables.size; i++) {
        struct key_type_pair pair = scope->scoped_variables.data[i];
        if (list_char_eq(&pair.field_name, &variable_name)) {
            *out = *pair.field_type;
            return 1;
        }
    }

    if (scope->parent != NULL) {
        return get_scoped_variable_type(scope->parent, variable_name, out);
    }

	return 0;
}

// TODO
int expression_is_pointer(struct expression *e, struct c_scope *scope) {
    switch (e->kind) {
        case LITERAL_EXPRESSION:
        {
            if (e->literal.kind == LITERAL_NAME) {
                struct type variable_type = {0};
                if (get_scoped_variable_type(scope, *e->literal.name, &variable_type)) {
                    for (size_t i = 0; i < variable_type.modifiers.size; i++) {
                        if (variable_type.modifiers.data[i].kind == POINTER_MODIFIER_KIND) {
                            return 1;
                        }
                    }
                }
            }

            break;
        }
        case UNARY_EXPRESSION:
        case BINARY_EXPRESSION:
        case GROUP_EXPRESSION:
        case FUNCTION_EXPRESSION:
            break;
    }

    return 0;
}

void write_unary_expression(struct unary_expression *e, struct c_scope *scope, FILE *file) {
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

    write_expression(e->expression, scope, file);
}

void write_binary_expression(struct binary_expression *e, struct c_scope *scope, FILE *file) {
    write_expression(e->l, scope, file);
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
        case LESS_THAN_BINARY:
            fprintf(file, " < ");
            break;
        case EQUAL_TO_BINARY:
            fprintf(file, " == ");
            break;
        case MULTIPLY_BINARY:
            fprintf(file, " * ");
            break;
        case DOT_BINARY:
        {
            if (expression_is_pointer(e->l, scope)) {
                fprintf(file, "->");
            } else {
                fprintf(file, ".");
            }
            break;
        }
        default:
            UNREACHABLE("binary operator not handled");
    }
    write_expression(e->r, scope, file);
}

void write_grouped_expression(struct expression *e, struct c_scope *scope, FILE *file) {
    fprintf(file, "(");
    write_expression(e, scope, file);
    fprintf(file, ")");
}

void write_function_expression(struct function_expression *e, struct c_scope *scope, FILE *file) {
    fprintf(file, "%s(", e->function_name->data);
    size_t param_count = e->params->size;
    for (size_t i = 0; i < param_count; i++) {
        write_expression(&e->params->data[i], scope, file);
        if (i < param_count - 1) {
            fprintf(file, ", ");
        }
    }
    fprintf(file, ")");
}

void write_expression(struct expression *e, struct c_scope *scope, FILE *file) {
    switch (e->kind) {
        case LITERAL_EXPRESSION:
            write_literal_expression(&e->literal, scope, file);
            return;
        case UNARY_EXPRESSION:
            write_unary_expression(&e->unary, scope, file);
            return;
        case BINARY_EXPRESSION:
            write_binary_expression(&e->binary, scope, file);
            return;
        case GROUP_EXPRESSION:
            write_grouped_expression(e->grouped, scope, file);
            return;
        case FUNCTION_EXPRESSION:
            write_function_expression(&e->function, scope, file);
            return;
        default:
            UNREACHABLE("expression kind not handled");
    }
}

int infer_type(struct expression *e, struct c_scope *scope, struct type *out) {
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
                    *out = (struct type) {
                        .kind = TY_STRUCT,
                        .name = e->literal.struct_enum.name,
                        .struct_type = (struct struct_type) {
                            .predefined = 1,
                        }
                    };
                    return 1;
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
					return get_scoped_variable_type(scope, *e->literal.name, out);
            }
        }
        case UNARY_EXPRESSION:
		{
			return infer_type(e->unary.expression, scope, out);
		}
        case BINARY_EXPRESSION:
        case GROUP_EXPRESSION:
        case FUNCTION_EXPRESSION:
            return 0;
    }
}

void write_statement(struct statement *s, struct c_scope *scope, FILE *file);

void write_type_default(struct type *type, FILE *file) {
    // TODO: derive default C values 
    fprintf(file, "0");
}

int variable_is_defined(struct c_scope *scope, struct list_char variable_name)
{
	for (size_t i = 0; i < scope->preceding_statements.len; i++) {
		struct statement s = scope->preceding_statements.statements[i];
		if (s.kind == BINDING_STATEMENT) {
			if (list_char_eq(&s.binding_statement.variable_name, &variable_name)) {
				return 1;
			}
		}
	}

	return 0;
}

void write_binding_statement(struct binding_statement *s, struct c_scope *scope, FILE *file) {
	if (!variable_is_defined(scope, s->variable_name)) {
		if (*&s->has_type) {
			write_type(&s->variable_type, file);
		} else {
			struct type inferred = {0};
			if (infer_type(&s->value, scope, &inferred)) {
				write_type(&inferred, file);
			}
		}
	}

    fprintf(file, " %s = ", s->variable_name.data);
    if (s->value.kind == LITERAL_EXPRESSION
        && s->value.literal.kind == LITERAL_NULL) {
        // TODO: check has_type
        write_type_default(&s->variable_type, file);
    } else {
        write_expression(&s->value, scope, file);
    }
    fprintf(file, ";");
}

void write_if_statement(struct if_statement *s, struct c_scope *scope, FILE *file) {
    fprintf(file, "if (");
    write_expression(&s->condition, scope, file);
    fprintf(file, ")");
    write_statement(s->success_statement, scope, file);
    if (s->else_statement != NULL) {
        fprintf(file, " else ");
        write_statement(s->else_statement, scope, file);
    }
}

void write_return_statement(struct expression *e, struct c_scope *scope, FILE *file) {
    fprintf(file, "return ");
    write_expression(e, scope, file);
    fprintf(file, ";");
}

struct_list(int);

void write_block_statement(struct list_statement *statements, struct c_scope *scope, FILE *file) {
    fprintf(file, "{");
    for (size_t i = 0; i < statements->size; i++) {
		struct c_scope inner_scope = (struct c_scope) {
			.preceding_statements = (struct statement_slice) {
				.statements = statements->data,
				.len = i
			},
			.parent = scope
		};
        write_statement(&statements->data[i], &inner_scope, file);
    }
    fprintf(file, "}");
}

void write_action_statement(struct expression *e, struct c_scope *scope, FILE *file) {
    write_expression(e, scope, file);
    fprintf(file, ";");
}

void write_while_statement(struct while_loop_statement *s, struct c_scope *scope, FILE *file) {
    fprintf(file, "while (");
    write_expression(&s->condition, scope, file);
    fprintf(file, ")");
    write_statement(s->do_statement, scope, file);
}

void write_type_declaration_statement(struct type_declaration_statement *s,
                                      struct c_scope *scope,
                                      FILE *file)
{
    write_type(&s->type, file);
    if (s->type.kind == TY_FUNCTION)
    {
		assert(s->statements != NULL);
        struct c_scope inner_scope = (struct c_scope) {
            .preceding_statements = (struct statement_slice) {
                .statements = NULL,
                .len = 0
            },
            .parent = scope,
            .scoped_variables = s->type.function_type.params
        };
        
        write_block_statement(s->statements, &inner_scope, file);
    }
}

void write_break_statement(struct c_scope *scope, FILE *file) {
    fprintf(file, "break;");
}

void write_include_statement(struct include_statement *s, struct c_scope *scope, FILE *file) {
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

void write_case_predicate(struct switch_pattern *p,
                          struct expression *e,
                          struct c_scope *scope,
                          FILE *file)
{
    switch (p->switch_pattern_kind) {
        case OBJECT_PATTERN_KIND:
            break;
        case ARRAY_PATTERN_KIND:
            break;
        case NUMBER_PATTERN_KIND:
            break;
        case STRING_PATTERN_KIND:
            break;
        case VARIABLE_PATTERN_KIND:
        {
            // struct type inferred_type = {0};
            // if (infer_type(e, scope, &inferred_type)) {
            //     write_type(&inferred_type, file);
            //     fprintf(file, " %s = ", p->variable_pattern.variable_name.data);
            //     write_expression(e, scope, file);
            //     fprintf(file, ";");
            // }
            fprintf(file, "if (1)");
            break;
        }
        case REST_PATTERN_KIND:
            UNREACHABLE("todo error handling");
    }
}

void write_case_statement(struct case_statement *s,
                          struct expression *e,
                          struct c_scope *scope,
                          FILE *file)
{
    write_case_predicate(&s->pattern, e, scope, file);
    fprintf(file, "{");
    write_statement(s->statement, scope, file);
    fprintf(file, "}");
}

void write_switch_statement(struct switch_statement *s, struct c_scope *scope, FILE *file) {
    for (size_t i = 0; i < s->cases.size; i++) {
        write_case_statement(&s->cases.data[i], &s->switch_expression, scope, file);
    }
}

void write_statement(struct statement *s, struct c_scope *scope, FILE *file) {
    switch (s->kind) {
        case BINDING_STATEMENT:
            write_binding_statement(&s->binding_statement, scope, file);
            break;
        case IF_STATEMENT:
            write_if_statement(&s->if_statement, scope, file);
            break;
        case RETURN_STATEMENT:
            write_return_statement(&s->expression, scope, file);
            break;
        case BLOCK_STATEMENT:
            write_block_statement(s->statements, scope, file);
            break;
        case ACTION_STATEMENT:
            write_action_statement(&s->expression, scope, file);
            break;
        case WHILE_LOOP_STATEMENT:
            write_while_statement(&s->while_loop_statement, scope, file);
            break;
        case TYPE_DECLARATION_STATEMENT:
            write_type_declaration_statement(&s->type_declaration, scope, file);
            break;
        case BREAK_STATEMENT:
            write_break_statement(scope, file);
            break;
        case INCLUDE_STATEMENT:
            write_include_statement(&s->include_statement, scope, file);
            break;
        case SWITCH_STATEMENT:
            write_switch_statement(&s->switch_statement, scope, file);
            break;
        default:
            UNREACHABLE("statement type not handled");
    }
}

struct generated_c_state {
    struct list_type *data_types;
    struct list_type *fn_types;
};

static void generate_c_file(struct rm_file file, struct generated_c_state *state) {
    FILE *program = fopen("c_output.c", "w");
    fprintf(program, "#include \"c_output.h\"\n");
    for (size_t i = 0; i < file.statements.size; i++) {
        struct statement s = file.statements.data[i];
		struct c_scope scope = {0};

		switch (s.kind) {
			case INCLUDE_STATEMENT:
        		write_statement(&file.statements.data[i], &scope, program);
				break;
			case TYPE_DECLARATION_STATEMENT:
			{
				switch (s.type_declaration.type.kind) {
					case TY_ENUM:
					case TY_STRUCT:
						list_append(state->data_types, s.type_declaration.type);
						break;
					case TY_FUNCTION:
						list_append(state->fn_types, s.type_declaration.type);
        				write_statement(&file.statements.data[i], &scope, program);
						break;
					case TY_PRIMITIVE:
					{
						fprintf(stderr, "woopssss");
						exit(-1);
					}
				}
				break;
			}
			case BINDING_STATEMENT:
			case IF_STATEMENT:
			case RETURN_STATEMENT:
			case BLOCK_STATEMENT:
			case ACTION_STATEMENT:
			case WHILE_LOOP_STATEMENT:
            case SWITCH_STATEMENT:
			case BREAK_STATEMENT:
			{
				fprintf(stderr, "woops");
				exit(-1);
			}
        } 
    }
}

void generate_c_header(struct generated_c_state *state) {
    FILE *header = fopen("c_output.h", "w");
    fprintf(header, "#ifndef C_OUTPUT_H\n#define C_OUTPUT_H\n");

    for (size_t i = 0; i < state->data_types->size; i++) {
        write_type(&state->data_types->data[i], header);
    }

    for (size_t i = 0; i < state->fn_types->size; i++) {
        write_type(&state->fn_types->data[i], header);
        fprintf(header, ";");
    }
    
    fprintf(header, "\n#endif");
}

void generate_c(struct rm_file file) {
    struct list_type data_types = list_create(type, 100);
    struct list_type fn_types = list_create(type, 100);
    struct generated_c_state state = {
        .data_types = &data_types,
        .fn_types = &fn_types
    };
    generate_c_file(file, &state);
    generate_c_header(&state);
}
