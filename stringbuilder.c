#include "stringbuilder.h"

int sb_init(int capacity, struct sb *ptr) {
	ptr->array = malloc(capacity);
    if (ptr->array == NULL) {
        return 0;
    }
	ptr->size = 0;
	ptr->capacity = capacity;
    return 1;
}

int sb_resize(int new_cap, struct sb *ptr) {
	char *new_arr;
	if (ptr->array != NULL) {
		new_arr = realloc(ptr->array, new_cap);
	} else {
        ptr->array = malloc(new_cap);
    }
    if (new_arr == NULL) {
        return 0;
    }
	ptr->array = new_arr;
	ptr->capacity = new_cap;
    return 1;
}

int sb_append(char const *buf, struct sb *ptr) {
    int len = strlen(buf);
	if (ptr->array != NULL) {
		if (ptr->size + len > ptr->capacity) {
			if (! sb_resize((ptr->size + len) * 2, ptr)) {
                return 0;
            }
		}
        strncpy(&ptr->array[ptr->size], buf, len);
		ptr->size += len;
	} else {
        return 0;
    }
    return 1;
}

char *sb_build(struct sb *ptr) {
	if (ptr->array != NULL) { 
		char *out_str = malloc(ptr->size + 1); // size doesn't include the NUL character
		strncpy(out_str, ptr->array, ptr->size);
		out_str[ptr->size] = 0;
		return out_str;
	}
	return NULL;
}

void sb_destroy(struct sb *ptr) {
	if (ptr->array != NULL) {
		free(ptr->array);
        ptr->array = NULL;
	}
	ptr->size = 0;
	ptr->capacity = 0;
}