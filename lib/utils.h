#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>

#define TODO(msg)                                                        \
    do {                                                                 \
        fprintf(stderr, "%s:%d: todo: `%s`\n", __FILE__, __LINE__, msg); \
        exit(1);                                                         \
    } while (0)

#define UNREACHABLE(msg)                                                        \
    do {                                                                        \
        fprintf(stderr, "%s:%d: unreachable: `%s`\n", __FILE__, __LINE__, msg); \
        exit(1);                                                                \
    } while (0)

unsigned long djb2_hash(char *input);

#endif
