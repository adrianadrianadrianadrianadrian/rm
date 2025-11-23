#include "utils.h"

void copy_list_char(struct list_char *dest, struct list_char *src) {
    for (size_t i = 0; i < src->size && src->data[i] != '\0'; i++) {
        list_append(dest, src->data[i]);
    }
}

void append_list_char_slice(struct list_char *dest, char *slice) {
    do {
        list_append(dest, *slice);
        slice++;
    } while (slice != NULL && *slice != '\0');
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
