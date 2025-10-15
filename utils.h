#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define LIST_NAME(ty) list_##ty

#define struct_list(ty)                                     \
    struct LIST_NAME(ty) {                                  \
        ty *data;                                           \
        size_t size;                                        \
        size_t capacity;                                    \
    }

#define list_create(ty, cap)                                \
    (struct LIST_NAME(ty)) {                                \
        .data = malloc(sizeof(ty) * cap),                   \
        .size = 0,                                          \
        .capacity = cap                                     \
    }

#define list_append(l, item)                                \
    do {                                                    \
        if ((l)->size + 1 > (l)->capacity) {                \
            size_t item_size = sizeof(*(l)->data);          \
            size_t new_capacity = 2 * (l)->capacity;        \
            void *data = malloc(item_size * new_capacity);  \
            memcpy(data, (l)->data, (l)->size * item_size); \
            free((l)->data);                                \
            (l)->data = data;                               \
            (l)->capacity = new_capacity;                   \
        }                                                   \
        (l)->data[(l)->size] = item;                        \
        (l)->size += 1;                                     \
    } while (0)

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
