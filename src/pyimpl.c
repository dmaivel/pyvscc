#include "pyimpl.h"
#include <string.h>

static inline void add_pyimpl_strlen(struct vscc_context *ctx)
{
    struct vscc_function *strlen = vscc_init_function(ctx, "pyimpl_strlen", SIZEOF_I64);

    struct vscc_register *strlen_string = vscc_alloc(strlen, "str", SIZEOF_PTR, IS_PARAMETER, NOT_VOLATILE);
    struct vscc_register *strlen_len = vscc_alloc(strlen, "length", SIZEOF_I64, NOT_PARAMETER, NOT_VOLATILE);
    struct vscc_register *strlen_deref = vscc_alloc(strlen, "deref_char", SIZEOF_I8, NOT_PARAMETER, NOT_VOLATILE);

    vscc_push0(strlen, O_STORE, strlen_len, 0);
    vscc_push2(strlen, O_DECLABEL, 0);
    vscc_push1(strlen, O_LOAD, strlen_deref, strlen_string);
    vscc_push0(strlen, O_CMP, strlen_deref, 0);
    vscc_push2(strlen, O_JE, 1);
    vscc_push0(strlen, O_ADD, strlen_string, 1);
    vscc_push0(strlen, O_ADD, strlen_len, 1);
    vscc_push2(strlen, O_JMP, 0);
    vscc_push2(strlen, O_DECLABEL, 1);
    vscc_push3(strlen, O_RET, strlen_len);
}

static inline void add_pyimpl_print_str(struct vscc_context *ctx)
{
    struct vscc_function *print = vscc_init_function(ctx, "pyimpl_print_str", SIZEOF_I64);

    struct vscc_register *print_string = vscc_alloc(print, "str", SIZEOF_PTR, IS_PARAMETER, NOT_VOLATILE);
    struct vscc_register *print_len = vscc_alloc(print, "length", SIZEOF_I64, NOT_PARAMETER, NOT_VOLATILE);

    struct vscc_syscall_args syscall_write = {
        .syscall_id = 1,
        .count = 3,

        .values = { 1, (uintptr_t)print_string, (size_t)print_len },
        .type = { M_IMM, M_REG, M_REG }
    };

    vscc_push3(print, O_PSHARG, print_string);
    vscc_push0(print, O_CALL, print_len, (uintptr_t)vscc_fetch_function_by_name(ctx, "pyimpl_strlen"));
    vscc_pushs(print, &syscall_write);
    vscc_push3(print, O_RET, print_len);
}

void pyimpl_append_to_context(struct vscc_context *ctx)
{
    vscc_alloc_global(ctx, "__name__", 16, false);

    add_pyimpl_strlen(ctx);
    add_pyimpl_print_str(ctx);
}

struct vscc_function *pyimpl_get(struct vscc_context *ctx, char *fn, enum pyimpl_implementation impl)
{
    static struct {
        char pyname[20];
        char implname[20];
        enum pyimpl_implementation impl;
    } table[] = {
        { "print", "pyimpl_print_str", PYIMPL_FIRST_ARG_STRING },
        { "print", "pyimpl_print_int", PYIMPL_FIRST_ARG_STRING },
        { "strlen", "pyimpl_strlen", PYIMPL_SINGLE_IMPL },
    };

    for (int i = 0; i < sizeof(table) / sizeof(*table); i++)
        if (strcmp(table[i].pyname, fn) == 0)
            return vscc_fetch_function_by_name(ctx, table[i].implname);
    return NULL;
}

enum pyimpl_implementation_status pyimpl_get_implementation_status(char *fn)
{
    static struct {
        char name[20];
        enum pyimpl_implementation_status status;
    } table[] = {
        { "print", PYIMPL_IMPLEMENTED_AND_MULTI_DEFINITION },
        { "strlen", PYIMPL_IMPLEMENTED_AND_SINGLE_DEFINITION }
    };

    for (int i = 0; i < sizeof(table) / sizeof(*table); i++)
        if (strcmp(table[i].name, fn) == 0)
            return table[i].status;
    return PYIMPL_NOT_IMPLEMENTED;
}