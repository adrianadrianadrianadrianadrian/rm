#ifndef LOWERING_C_H
#define LOWERING_C_H

#include "../context.h"
#include "../parser.h"

void generate_c(struct parsed_file *parsed_file,
                struct context *context);

#endif
