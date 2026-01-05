#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include "ast.h"
#include "context.h"
#include "error.h"

int type_check(struct rm_program *program, struct error *error);

#endif
