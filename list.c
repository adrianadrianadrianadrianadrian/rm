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
    
