#ifndef TYPE_INFERENCE_H
#define TYPE_INFERENCE_H

#include "context.h"
#include "ast.h"
#include "utils.h"

int infer_expression_type(struct expression *e,
                          struct global_context *global_context,
                          struct list_scoped_variable *scoped_variables,
                          struct type *out,
                          struct list_char *error);

#endif
