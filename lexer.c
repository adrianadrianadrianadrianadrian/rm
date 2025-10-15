#include "list.h"
#include "lexer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

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

int is_keyword(struct list_char *ident, enum token_type *out) {
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
                    .token_type = OPEN_ROUND_PAREN,
                    .position = b->current_position
                };
                return 1;
            }
            case ')': {
                *out = (struct token) {
                    .token_type = CLOSE_ROUND_PAREN,
                    .position = b->current_position
                };
                return 1;
            }
            case '{': {
                *out = (struct token) {
                    .token_type = OPEN_CURLY_PAREN,
                    .position = b->current_position
                };
                return 1;
            }
            case '}': {
                *out = (struct token) {
                    .token_type = CLOSE_CURLY_PAREN,
                    .position = b->current_position
                };
                return 1;
            }
            case '[': {
                *out = (struct token) {
                    .token_type = OPEN_SQUARE_PAREN,
                    .position = b->current_position
                };
                return 1;
            }
            case ']': {
                *out = (struct token) {
                    .token_type = CLOSE_SQUARE_PAREN,
                    .position = b->current_position
                };
                return 1;
            }
            case '=': {
                *out = (struct token) {
                    .token_type = EQ,
                    .position = b->current_position
                };
                return 1;
            }
            case '!': {
                *out = (struct token) {
                    .token_type = BANG,
                    .position = b->current_position
                };
                return 1;
            }
            case '%': {
                *out = (struct token) {
                    .token_type = MOD,
                    .position = b->current_position
                };
                return 1;
            }
            case '/': {
                *out = (struct token) {
                    .token_type = DIV,
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
                    .token_type = PLUS,
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
                    .token_type = MINUS,
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

                enum token_type keyword;
                if (is_keyword(ident, &keyword)) {
                    *out = (struct token) {
                        .token_type = keyword,
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

struct token_buffer create_token_buffer(FILE *fstream) {
    struct file_buffer b = create_file_buffer(fstream);
    struct list_token tokens = list_create(token, b.size);
    struct token tok;

    while (next_token(&b, &tok)) {
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
