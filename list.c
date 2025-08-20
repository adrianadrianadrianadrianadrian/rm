#define LIST_NAME(ty) list_##ty

#define LIST(ty)                                                               \
    struct LIST_NAME(ty) {                                                     \
        ty *data;                                                              \
        size_t size;                                                           \
        size_t capacity;                                                       \
    }

#define CREATE_LIST(ty)                                                        \
    struct LIST_NAME(ty) create_list_##ty(size_t capacity) {                   \
        ty *data = malloc(sizeof(*data) * capacity);                           \
        struct LIST_NAME(ty) l;                                                \
        l.data = data;                                                         \
        l.size = 0;                                                            \
        l.capacity = capacity;                                                 \
        return l;                                                              \
    }

#define APPEND_LIST(ty)                                                        \
    void append_list_##ty(struct LIST_NAME(ty) * l, ty t) {                    \
        if (l->size + 1 > l->capacity) {                                       \
            int new_capacity = 2 * l->capacity;                                \
            ty *data = malloc(sizeof(*data) * new_capacity);                   \
            memcpy(data, l->data, l->size * sizeof(*data));                    \
            free(l->data);                                                     \
            l->data = data;                                                    \
            l->capacity = new_capacity;                                        \
        }                                                                      \
        l->data[l->size] = t;                                                  \
        l->size += 1;                                                          \
    }
