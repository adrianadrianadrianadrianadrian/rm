#ifndef SOUNDNESS_H
#define SOUNDNESS_H

#include "context.h"
#include "error.h"

int soundness_check(struct rm_program *program, struct error *error);

#endif
