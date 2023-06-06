#include "pybuild.h"
#include "lexer.h"
#include "pyimpl.h"

#include <vscc.h>

#include <util/list.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define fail_if(x, ...) \
    if (x) { \
        *status = false; \
        printf(__VA_ARGS__); \
        return; \
    }

// static void parse_...(struct pybuild_context *ctx, struct lexer_token *start_token, struct lexer_token *end_token, bool *status)
typedef void(*parser_impl_fnptr)(struct pybuild_context *ctx, struct lexer_token *start_token, struct lexer_token *end_token, bool *status);

static struct lexer_token *next(struct lexer_token *c)
{
    if (c->next->type == TOKEN_WHITESPACE)
        return next(c->next);
    return c->next;
}

static size_t parse_size(char *str)
{
    static const struct {
        char cmp[12];
        size_t size;
    } table[] = {
        { .cmp = "byte", .size = sizeof(uint8_t) },
        { .cmp = "word", .size = sizeof(uint16_t) },
        { .cmp = "dword", .size = sizeof(uint32_t) },
        { .cmp = "qword", .size = sizeof(uint64_t) }
    };

    for (int i = 0; i < sizeof(table) / sizeof(*table); i++)
        if (strcmp(str, table[i].cmp) == 0)
            return table[i].size;
    return 0;
}

static void parse_definition(struct pybuild_context *ctx, struct lexer_token *start_token, struct lexer_token *end_token, bool *status)
{
    /* next token is [identifier] */
    struct lexer_token *token = next(start_token);
    ctx->current_function = vscc_init_function(&ctx->vscc_ctx, token->contents, ctx->default_size);

    /* next two tokens are [(, identifier]*/
    token = next(next(token));
    while (token != end_token && token && token->type != TOKEN_CLOSE_PAREN) {
        if (token->type == TOKEN_COMMA || token->type == TOKEN_WHITESPACE) {
            token = token->next;
            continue;
        }
        
        switch (next(token)->type) {
        case TOKEN_COMMA:
        case TOKEN_CLOSE_PAREN:
            vscc_alloc(ctx->current_function, token->contents, ctx->default_size, true, true);
            break;
        case TOKEN_COLON:
            vscc_alloc(ctx->current_function, token->contents, parse_size(next(next(token))->contents), true, true);
            token = next(next(token));
            break;
        default:
            /* to-do: fail? */
            break;
        }
        token = token->next;
    }

    token = next(token);
    switch (token->type) {
    case TOKEN_COLON:
        return;
    case TOKEN_RARROW:
        token = next(token);
        ctx->current_function->return_size = parse_size(token->contents);
        break;
    default:
        fail_if(true, "err: unexpected token at end of def, '%s'\n", token->contents);
    }

    fail_if(next(token)->type != TOKEN_COLON, "err: expected ':' at end of function '%s'\n", ctx->current_function->symbol_name);
}

static struct vscc_register *get_variable(struct pybuild_context *ctx, char *name)
{
    struct vscc_register *res = vscc_fetch_register_by_name(ctx->current_function, name);
    res = res ? res : vscc_fetch_global_register_by_name(&ctx->vscc_ctx, name);
    if (res == NULL)
        return vscc_alloc(ctx->current_function, name, ctx->default_size, false, true);
    return res;
}

static char *generate_name_for_local_global(struct vscc_function *current_function, char *name)
{
    static char res[64];
    static int counter = 0;
    char temp[16];

    strcpy(res, "__autogen_");
    strcat(res, current_function->symbol_name);
    strcat(res, "_");
    if (name)
        strcat(res, name);
    else {
        sprintf(temp, "%d", counter++);
        strcat(res, temp);
    }
    return res;
}

static void queue_memcpy(struct pybuild_context *ctx, char *name, void *data, size_t length)
{
    struct pybuild_memcpy *res = vscc_list_alloc((void**)&ctx->memcpy_queue, 0, sizeof(struct pybuild_memcpy));
    res->dst = name;
    res->src = data;
    res->length = length;
}

static struct vscc_register *create_string(struct pybuild_context *ctx, struct lexer_token *token, struct vscc_register *dst)
{
    struct vscc_register *raw = vscc_alloc_global(&ctx->vscc_ctx, generate_name_for_local_global(ctx->current_function, NULL), strlen(token->contents) - 1, true);
    struct vscc_register *ptr = dst != NULL ? dst : vscc_alloc(ctx->current_function, generate_name_for_local_global(ctx->current_function, NULL), sizeof(void*), false, true);
    queue_memcpy(ctx, raw->symbol_name, &token->contents[1], strlen(token->contents) - 2);
    vscc_push1(ctx->current_function, O_LEA, ptr, raw);
    return ptr;
}

static void parse_call(struct pybuild_context *ctx, struct lexer_token *start_token, struct lexer_token *end_token, bool *status)
{
    /*
     * check if implementation of function exists
     */
    struct vscc_function *callee = NULL;
    struct lexer_token *token = next(next(start_token));

    switch (pyimpl_get_implementation_status(start_token->contents)) {
    case PYIMPL_NOT_IMPLEMENTED:
        callee = vscc_fetch_function_by_name(&ctx->vscc_ctx, start_token->contents);
        break;
    case PYIMPL_IMPLEMENTED_AND_SINGLE_DEFINITION:
        callee = pyimpl_get(&ctx->vscc_ctx, start_token->contents, PYIMPL_SINGLE_IMPL);
        break;
    case PYIMPL_IMPLEMENTED_AND_MULTI_DEFINITION:
        callee = pyimpl_get(&ctx->vscc_ctx, start_token->contents, token->type == TOKEN_LITERAL ? PYIMPL_FIRST_ARG_INT : PYIMPL_FIRST_ARG_STRING);
        break;
    }

    fail_if(callee == NULL, "err: could not find function '%s'\n", start_token->contents);
    while (token != end_token && token && token->type != TOKEN_CLOSE_PAREN) {
        if (token->type == TOKEN_COMMA || token->type == TOKEN_WHITESPACE) {
            token = token->next;
            continue;
        }
        
        switch (token->type) {
        case TOKEN_IDENTIFIER:
            vscc_push3(ctx->current_function, O_PSHARG, get_variable(ctx, token->contents));
            break;
        case TOKEN_LITERAL:
            vscc_push2(ctx->current_function, O_PSHARG, atoi(token->contents));
            break;
        case TOKEN_STRING:;
            vscc_push3(ctx->current_function, O_PSHARG, create_string(ctx, token, NULL));
            break;
        default:
            fail_if(true, "err: expected parameter, got this instead: '%s'\n", token->contents);
        }
        token = token->next;
    }

    if (ctx->return_reg)
        ctx->return_reg->size = callee->return_size;

    _vscc_call(ctx->current_function, callee, ctx->return_reg ? ctx->return_reg : vscc_alloc(ctx->current_function, generate_name_for_local_global(ctx->current_function, NULL), ctx->default_size, false, true));
}

static void parse_assignment(struct pybuild_context *ctx, struct lexer_token *start_token, struct lexer_token *end_token, bool *status)
{
    struct vscc_register *dst = get_variable(ctx, start_token->contents);

    switch (next(start_token)->type) {
    case TOKEN_EQUAL:
        if (next(next(start_token))->type == TOKEN_LITERAL)
            vscc_push0(ctx->current_function, O_STORE, dst, atoi(next(next(start_token))->contents));
        else if (next(next(start_token))->type == TOKEN_STRING)
            create_string(ctx, next(next(start_token)), dst);
        else if (next(next(start_token))->type == TOKEN_IDENTIFIER) {
            if (next(next(next(start_token)))->type == TOKEN_OPEN_PAREN) {
                ctx->return_reg = dst;
                parse_call(ctx, next(next(start_token)), end_token, status);
                ctx->return_reg = NULL;
            }
            else
                vscc_push1(ctx->current_function, O_STORE, dst, get_variable(ctx, start_token->contents));
        }
        break;
    default:
        fail_if(true, "err: unexpected assignment\n");
    }
}

static void parse_identifier(struct pybuild_context *ctx, struct lexer_token *start_token, struct lexer_token *end_token, bool *status)
{
    switch (next(start_token)->type) {
    case TOKEN_OPEN_PAREN:
        parse_call(ctx, start_token, end_token, status);
        break;
    case TOKEN_EQUAL:
        parse_assignment(ctx, start_token, end_token, status);
        break;
    default:
        fail_if(true, "err: unexcepted identifier '%s'\n", start_token->contents);
    }
}

static void parse_return(struct pybuild_context *ctx, struct lexer_token *start_token, struct lexer_token *end_token, bool *status)
{
    struct lexer_token *token = next(start_token);

    switch (token->type) {
    case TOKEN_LITERAL:
        vscc_push2(ctx->current_function, O_RET, atoi(token->contents));
        break;
    case TOKEN_IDENTIFIER:
        /* to-do: support returning functions/expressions */
        vscc_push3(ctx->current_function, O_RET, get_variable(ctx, token->contents));
        break;
    default:
        fail_if(true, "err: unexcepted identifier following return, '%s'\n", start_token->contents);
    }
}

static void parse_if(struct pybuild_context *ctx, struct lexer_token *start_token, struct lexer_token *end_token, bool *status)
{
    struct lexer_token *dst_token = next(start_token);
    struct lexer_token *operation_token = next(dst_token);
    struct lexer_token *src_token = next(operation_token);

    struct vscc_register *dest = get_variable(ctx, dst_token->contents);

    switch (src_token->type) {
    case TOKEN_LITERAL:
        vscc_push0(ctx->current_function, O_CMP, dest, atoi(src_token->contents));
        break;
    default:
        /* to-do: fail? */
        break;
    }

    switch (operation_token->type) {
    case TOKEN_EQUALS:
        vscc_push2(ctx->current_function, O_JNE, ctx->current_label);
        break;
    case TOKEN_NEQUALS:
        vscc_push2(ctx->current_function, O_JE, ctx->current_label);
        break;
    default:
        /* to-do: fail? */
        break;
    }

    ctx->labelc++;
}

bool parse(struct pybuild_context *ctx, struct lexer_token *lex_tokens)
{
    int expected_tabs = 0;
    bool status = true;
    bool def = false;
    
    struct lexer_token *stop_token;
    for (struct lexer_token *token = lex_tokens; token; token = stop_token->next) {
        if (token->type == TOKEN_NEWLINE)
            continue;
    
        /* 
         * loop thru line 
         */
        for (stop_token = token; stop_token->next && stop_token->type != TOKEN_NEWLINE; stop_token = stop_token->next);

        /*
         * find number of tabs
         */
        int tabs_found = 0;
        struct lexer_token *start_token;
        for (start_token = token; start_token != stop_token; start_token = start_token->next) {
            if (start_token->type != TOKEN_TAB)
                break;
            tabs_found++;
        }

        /*
         * comments to be ignored
         */
        if (start_token->type == TOKEN_COMMENT)
            continue;

        if (tabs_found == 0 && def)
            def = false;

        /*
         * figure out tab information
         */
        if (ctx->labelc + 1 != tabs_found && def) {
            ctx->labelc--;
            vscc_push2(ctx->current_function, O_DECLABEL, ctx->current_label++);
        }

        /*
         * start parsing 
         */
        switch (start_token->type) {
        case TOKEN_DEF:
            def = true;
            ctx->current_label = 0;
            parse_definition(ctx, start_token, stop_token, &status);
            break;
        case TOKEN_IDENTIFIER:
            parse_identifier(ctx, start_token, stop_token, &status);
            break;
        case TOKEN_RETURN:
            parse_return(ctx, start_token, stop_token, &status);
            break;
        case TOKEN_IF:
            parse_if(ctx, start_token, stop_token, &status);
            break;
        default:
            /* to-do: fail? */
            break;
        }
    }

    /* if (ctx->labelc != 0) {
        ctx->labelc--;
        vscc_push2(ctx->current_function, O_DECLABEL, ctx->current_label++);
    } */

    return status;
}

static uintptr_t get_offset_from_symbol(struct vscc_symbol *symbols, char *symbol_name)
{
    for (struct vscc_symbol *symbol = symbols; symbol; symbol = symbol->next) {
        if (strstr(symbol->symbol_name, symbol_name) != NULL)
            return symbol->offset;
    }
    return -1;
}

uintptr_t build(struct pybuild_context *ctx)
{
    struct vscc_codegen_interface interface = { 0 };
    vscc_codegen_implement_x64(&interface, ABI_SYSV);

    vscc_codegen(&ctx->vscc_ctx, &interface, &ctx->compiled_data, true);

    for (struct pybuild_memcpy *blk = ctx->memcpy_queue; blk; blk = blk->next) {
        uintptr_t offset = get_offset_from_symbol(ctx->compiled_data.symbols, blk->dst);
        memcpy(ctx->compiled_data.buffer + offset, blk->src, blk->length);
    }

    return get_offset_from_symbol(ctx->compiled_data.symbols, ctx->entry_name);
}