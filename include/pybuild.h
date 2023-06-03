#ifndef _PARSER_H_
#define _PARSER_H_

#include <vscc.h>
#include "asm/assembler.h"
#include "ir/intermediate.h"
#include "lexer.h"

struct pybuild_memcpy {
    struct pybuild_memcpy *next;

    char *dst;
    void *src;
    size_t length;
};

struct pybuild_context {
    struct vscc_context vscc_ctx;
    struct vscc_compiled_data compiled_data;
    char *entry_name;

    int current_label;
    int labelc;

    struct vscc_function *current_function;
    size_t default_size;

    struct pybuild_memcpy *memcpy_queue;
};

bool parse(struct pybuild_context *ctx, struct lexer_token *lex_tokens);
uintptr_t build(struct pybuild_context *ctx);

#endif