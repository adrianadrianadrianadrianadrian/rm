#include "utils.h"

unsigned long djb2_hash(char *input)
{
    unsigned long hash = 5381;
    char c;

    while ((c = *input++))
    {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}
