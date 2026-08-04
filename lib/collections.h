#ifndef COLLECTIONS_H
#define COLLECTIONS_H

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
        .capacity = cap > 0 ? cap : 1                       \
    }

#define list_append(l, item)                                \
    do {                                                    \
        if ((l)->size + 1 > (l)->capacity) {                \
            size_t item_size = sizeof(*(l)->data);          \
            (l)->capacity *= 2;                             \
            void *data = malloc(item_size * (l)->capacity); \
            memcpy(data, (l)->data, (l)->size * item_size); \
            free((l)->data);                                \
            (l)->data = data;                               \
        }                                                   \
        (l)->data[(l)->size] = item;                        \
        (l)->size += 1;                                     \
    } while (0)

struct_list(int);
static struct list_int *create_boxed_list_int(size_t cap)
{
    struct list_int *output = malloc(sizeof(*output));
    *output = list_create(int, cap);
    return output;
}

// Look up table (LUT)
#define LUT_NAME(ty) lut_##ty

#define struct_lut(ty)                                      \
    struct LUT_NAME(ty) {                                   \
        ty *data;                                           \
        size_t capacity;                                    \
        struct list_int *keys;                              \
    }

#define lut_create(ty, cap)                                 \
    (struct LUT_NAME(ty)) {                                 \
        .data = malloc(sizeof(ty) * cap),                   \
        .capacity = cap > 0 ? cap : 1,                      \
        .keys = create_boxed_list_int(cap)                  \
    }

#define lut_add(l, i, item)                                     \
    do {                                                        \
        if ((i) + 1 > (l)->capacity) {                          \
            size_t item_size = sizeof(*(l)->data);              \
            size_t new_capacity = 2*(i);                        \
            void *data = malloc(new_capacity * item_size);      \
            memcpy(data, (l)->data, (l)->capacity * item_size); \
            free((l)->data);                                    \
            (l)->data = data;                                   \
            (l)->capacity = new_capacity;                       \
        }                                                       \
        (l)->data[(i)] = item;                                  \
        list_append((l)->keys, (i));                            \
    } while (0)

#define lut_get(l, i) (l)->data[i]

struct_list(char);

typedef struct list_char string;
struct_list(string);

void copy_list_char(struct list_char *dest, struct list_char *src);
void append_list_char_slice(struct list_char *dest, char *slice);
int list_char_eq(struct list_char *l, struct list_char *r);

#endif
