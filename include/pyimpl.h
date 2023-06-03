#ifndef _PYIMPL_H_
#define _PYIMPL_H_

#include "ir/intermediate.h"
#include "lexer.h"
#include <vscc.h>

enum pyimpl_implementation_status {
    PYIMPL_NOT_IMPLEMENTED,
    PYIMPL_IMPLEMENTED_AND_SINGLE_DEFINITION,
    PYIMPL_IMPLEMENTED_AND_MULTI_DEFINITION
};

enum pyimpl_implementation {
    PYIMPL_SINGLE_IMPL,
    PYIMPL_FIRST_ARG_INT,
    PYIMPL_FIRST_ARG_STRING,
};

void pyimpl_append_to_context(struct vscc_context *ctx);
struct vscc_function *pyimpl_get(struct vscc_context *ctx, char *fn, enum pyimpl_implementation impl);

enum pyimpl_implementation_status pyimpl_get_implementation_status(char *fn);

#endif