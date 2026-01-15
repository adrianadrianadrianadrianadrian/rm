#ifndef TYPE_INFERENCE_H
#define TYPE_INFERENCE_H

#include "ast.h"
#include "context.h"
#include "error.h"

int infer_expression_type(struct expression *e,
                          struct global_context *global_context,
                          struct context *context,
                          struct list_scoped_variable *scoped_variables,
                          struct type *out,
                          struct list_char *error);

int infer_full_type(struct type *incomplete_type,
                    struct global_context *global_context,
                    struct list_scoped_variable *scoped_variables,
                    struct type *out,
                    struct list_char *error);

#endif
