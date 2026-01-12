#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include "ast.h"
#include "context.h"
#include "error.h"

int type_check(struct parsed_file *parsed_file, struct context *context, struct error *error);

#endif
