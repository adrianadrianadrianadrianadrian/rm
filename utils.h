#ifndef UTILS_H
#define UTILS_H

#include "list.h"
#include <stdio.h>

#define TODO(msg) \
    fprintf(stderr, "%s:%d: todo: `%s`\n", __FILE__, __LINE__, msg); \
    exit(1);

#define UNREACHABLE(msg) \
    fprintf(stderr, "%s:%d: unreachable: `%s`\n", __FILE__, __LINE__, msg); \
    exit(1);

struct_list(char);
void copy_list_char(struct list_char *dest, struct list_char *src);
void append_list_char_slice(struct list_char *dest, char *slice);
int list_char_eq(struct list_char *l, struct list_char *r);

#endif
