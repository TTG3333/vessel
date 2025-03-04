#ifndef _STRINGBUILDER_
#define _STRINGBUILDER_

#include <stdlib.h>
#include <string.h>

typedef struct sb {
    char *array;
    int size;
    int capacity;
} sb;

int sb_init(int capacity, struct sb *ptr);
int sb_append(char const *buf, struct sb *ptr);
char *sb_build(struct sb *ptr);
void sb_destroy(struct sb *ptr);

#endif