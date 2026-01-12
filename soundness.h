#ifndef SOUNDNESS_H
#define SOUNDNESS_H

#include "context.h"
#include "parser.h"
#include "error.h"

int soundness_check(struct parsed_file *parsed_file, struct context *context, struct error *error);

#endif
