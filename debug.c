#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "lib/collections.h"

struct lexer {
    char *file;
    size_t position;
    size_t size;
};

typedef struct struct_field {
    struct list_char field_name;
    struct list_char type;
    int is_pointer;
} struct_field;

struct_list(struct_field);

typedef struct c_struct {
    struct list_char struct_name;
    struct list_struct_field fields;
} c_struct;

struct_list(c_struct);

struct debug_state {
    struct list_c_struct structs;
};

int is_primitive(struct list_char type)
{
    return 0;
    //return c_type != STRUCT && c_type != ENUM;
}

static int write_debug_file(struct debug_state *s, FILE *fp);

int generate_debug_file(struct debug_state *s,
                        char *directory,
                        char *name)
{
    size_t directory_len = strlen(directory);
    size_t name_len = strlen(name);

    char *n = malloc(directory_len + name_len + 1);
    assert(n);
    memcpy(n, directory, directory_len);
    n[directory_len] = '/';
    memcpy(n + directory_len + 1, name, name_len);

    FILE *fp = fopen(n, "w");

    if (!fp) return 0;
    if (!write_debug_file(s, fp)) return 0;
    if (fclose(fp)) return 0;

    return 1;
}

void write_debug_function(struct c_struct *c_struct,
                          struct debug_state *state,
                          struct list_char *out)
{
    append_list_char_slice(out, c_struct->struct_name.data);
    append_list_char_slice(out, "_debug(struct ");
    append_list_char_slice(out, c_struct->struct_name.data);
    append_list_char_slice(out, "* data) {");
    for (size_t i = 0; i < c_struct->fields.size; i++) {
        struct struct_field this = c_struct->fields.data[i];
        struct list_char type = this.type;
        if (is_primitive(type)) {
            // TODO printf based off type.
            append_list_char_slice(out, "printf(\"%d\", this.field_name);");
        } else {
            append_list_char_slice(out, this.field_name.data);
            append_list_char_slice(out, "_debug(data->");
            append_list_char_slice(out, this.field_name.data);
            append_list_char_slice(out, ");");
        }
        list_append(out, '\n');
    }
    append_list_char_slice(out, "}");
    list_append(out, '\0');
}

int is_whitespace(char c)
{
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

int is_special_char(char c)
{
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

int is_special_or_whitespace(char c)
{
    return is_special_char(c) || is_whitespace(c);
}

void eat_whitespace(struct lexer *lexer)
{
    for (;;) {
        char c = lexer->file[lexer->position];
        if (is_whitespace(c)) {
            lexer->position++;
            c = lexer->file[lexer->position];
            continue;
        }

        break;
    }
}

char current(struct lexer *lexer)
{
    return lexer->file[lexer->position];
}

int parse_ident(struct lexer *lexer,
                struct list_char *out)
{
    eat_whitespace(lexer);
    int success = 0;
    char c = current(lexer);
    while (!is_special_or_whitespace(c) && lexer->position <= lexer->size) {
        list_append(out, c);
        lexer->position++;
        c = current(lexer);
        success = 1;
    }
    if (success) {
        list_append(out, '\0');
    }
    return success;
}

int parse_field(struct lexer *lexer,
                struct struct_field *out)
{
    struct list_char tmp = list_create(char, 10);
    if (!parse_ident(lexer, &tmp)) return 0;
    if (!strcmp(tmp.data, "struct")
        || !strcmp(tmp.data, "unsigned")
        || !strcmp(tmp.data, "enum")) {
        if (!parse_ident(lexer, &out->type)) return 0;
    } else if (!strcmp(tmp.data, "union")) {
        size_t i = lexer->position;
        int anonymous_union = 0;
        for (;;) {
            if (!anonymous_union && lexer->file[i] == '}') break;
            if (lexer->file[i] == '{') {
                anonymous_union = 1;
            }
            i++;
            if (anonymous_union && lexer->file[i] == '}') {
                lexer->position = i;
                break;
            }
        }
        if (!anonymous_union) {
            if (!parse_ident(lexer, &out->type)) return 0;
        } else {
            return 1;
        }
    } else {
        out->type = tmp;
    }
    eat_whitespace(lexer);
    if (current(lexer) == '*') {
        out->is_pointer = 1;
        lexer->position++;
    }
    if (!parse_ident(lexer, &out->field_name)) return 0;
    eat_whitespace(lexer);
    if (current(lexer) == ';') {
        lexer->position++;
        return 1;
    }

    return 0;
}

void parse_struct(struct lexer *lexer,
                  struct debug_state *s)
{
    int success = 0;
    struct c_struct output = (struct c_struct) {
        .struct_name = list_create(char, 10),
        .fields = list_create(struct_field, 10)
    };

    if (!parse_ident(lexer, &output.struct_name)) return;
    eat_whitespace(lexer);
    if (current(lexer) != '{') return;
    lexer->position++;
    while (current(lexer) != '}') {
        struct struct_field f = (struct_field) {
            .type = list_create(char, 10),
            .field_name = list_create(char, 10),
            .is_pointer = 0
        };
        if (!parse_field(lexer, &f)) return;
        list_append(&output.fields, f);
        success = 1;
    }
    if (success) {
        list_append(&s->structs, output);
    }
}

int parse_structs(struct lexer lexer,
                  struct debug_state *s)
{
    while (lexer.position <= lexer.size) {
        char c = lexer.file[lexer.position];
        switch (c) {
            case '/': {
                if (lexer.position + 1 > lexer.size) break;
                if (lexer.file[lexer.position + 1] == '/') {
                    for (;;) {
                        lexer.position++;
                        if (lexer.file[lexer.position] == '\n') {
                            break;
                        }
                    }
                }
                break;
            }
            case '#': {
                for (;;) {
                    lexer.position++;
                    if (lexer.file[lexer.position] == '\n') {
                        break;
                    }
                }
                break;
            }
            default: {
                struct list_char ident = list_create(char, 10);
                if (parse_ident(&lexer, &ident)) {
                    if (!strcmp(ident.data, "struct")) {
                        parse_struct(&lexer, s);
                    }
                }
                break;
            }
        }
        lexer.position++;
    }
    return 1;
}

struct lexer create_lexer(FILE *fp)
{
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    struct lexer output = (struct lexer) {
        .file = malloc(size),
        .position = 0,
        .size = size
    };

    size_t read_amount = fread(output.file, sizeof(char), size, fp);
    assert(read_amount == (size_t)size);
    return output;
}

int apply_file(struct debug_state *s,
               char *file)
{
    FILE *fp = fopen(file, "r");
    if (!fp) goto error;
    struct lexer lexer = create_lexer(fp);
    if (!parse_structs(lexer, s)) goto error;
    for (size_t i = 0; i < s->structs.size; ++i) {
        printf("%s\n", s->structs.data[i].struct_name.data);
        struct list_struct_field fields = s->structs.data[i].fields;
        for (size_t j = 0; j < fields.size; ++j) {
            printf("\t%s %s\n", fields.data[j].type.data, fields.data[j].field_name.data);
        }
    }
    if (fclose(fp)) goto error;
    return 1;

    error: {
        if (!fp) {
            fclose(fp);
        }

        return 0;
    }
}

static int write_debug_file(struct debug_state *s,
                            FILE *fp)
{
    return 1;
}
