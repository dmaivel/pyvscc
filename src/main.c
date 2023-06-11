#include <vscc.h>

#include "ir/intermediate.h"
#include "opt/opt.h"
#include "util.h"
#include "lexer.h"
#include "pyimpl.h"
#include "pybuild.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>

#define LOG_DEFAULT "\033[0m"
#define LOG_RED "\033[0;31m"
#define LOG_GREEN "\033[0;32m"
#define LOG_YELLOW "\033[0;33m"
#define LOG_CYAN "\033[0;36m"

typedef uint64_t(*entry_point_fnptr)();

static const char *usage = 
    "usage: pyvscc [-h] [-i FILE_PATH] [-e ENTRY_POINT] [-m SIZE] [-s SIZE] [-o] [-p] [-u]\n"
    "\n"
    "options:\n"
    "  -h                   display help information\n"
    "  -i [FILE_PATH]       input file path\n"
    "  -e [ENTRY_POINT]     specify entry function (if not specified, searches for any function containing 'main')\n"
    "  -m [SIZE]            max amount of bytes program may allocate (default: 4096 bytes)\n"
    "  -s [SIZE]            amount of bytes variables/functions with an unspecified type take up (default: 8 bytes)\n"
    "  -o                   enable optimizations\n"
    "  -p                   print performance information\n";

struct args {
    char *filepath;
    char *entry;
    size_t default_size;
    size_t max_mem;
    bool optimize;
    bool perf;
};

static void *map(struct vscc_codegen_data *compiled)
{
    void *exe = mmap(NULL, compiled->length, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    memcpy(exe, compiled->buffer, compiled->length);
    return exe;
}

static int64_t time_ms(void) 
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
}

static int64_t time_us(void) 
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("%s", usage);
        return 0;
    }

    struct args program_args = { 
        .filepath = NULL,
        .entry = "main",
        .default_size = sizeof(uint64_t),
        .max_mem = 4096,
        .optimize = false,
        .perf = false
    };

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'h':
                printf("%s", usage);
                return 0;
            case 'i':
                program_args.filepath = argv[i + 1];
                i++;
                break;
            case 'e':
                program_args.entry = argv[i + 1];
                i++;
                break;
            case 'm':
                program_args.max_mem = atoi(argv[i + 1]);
                i++;
                break;
            case 's':
                program_args.default_size = atoi(argv[i + 1]);
                i++;
                break;
            case 'o':
                program_args.optimize = true;
                break;
            case 'p':
                program_args.perf = true;
                break;
            default:
                printf("wrn: unknown argument '%s'\n", argv[i]);
            }
        } else {
            printf("wrn: unknown argument '%s'\n", argv[i]);
        }
    }

    /*
     * get file
     */
    char *buffer = file_to_str(program_args.filepath);
    if (buffer == NULL) {
        printf("err: could not open file '%s'\n", program_args.filepath);
        return 0;
    }
    
    /*
     * basic setup and lex input
     */
    struct lexer_token *tokens = str_to_tokens(buffer);
    struct pybuild_context ctx = {  
        .vscc_ctx = { 0 },
        .compiled_data = { 0 },
        .entry_name = program_args.entry,

        .current_label = 0,
        .labelc = 0,

        .current_function = NULL,
        .return_reg = NULL,
        .default_size = program_args.default_size,

        .memcpy_queue = NULL,
        .branch_queue = NULL
    };

    /*
     * perf helper
     */
    int64_t start_time = 0;
    int64_t end_time = 0;

    /*
     * append python functions & environmental variables
     */
    pyimpl_append_to_context(&ctx.vscc_ctx);

    /*
     * parse file and construct intermediate representation
     */
    start_time = time_us();
    bool status = parse(&ctx, tokens);
    end_time = time_us();
    if (!status) {
        printf("err: failed to compile\n");
        return 0;
    }

    /*
     * perform optimizations
     */
    if (program_args.optimize) {
        for (struct vscc_function *fn = ctx.vscc_ctx.function_stream; fn; fn = fn->next)
            vscc_optfn_elim_dead_store(fn);
    }

    /*
     * perf numbers
     */
    if (program_args.perf)
        printf("pyvscc: parsed and constructed intermediate representation in %ld us\n", end_time - start_time);

    /*
     * compile code and obtain entry point offset
     */
    start_time = time_us();
    uintptr_t entry_offset = build(&ctx);
    end_time = time_us();
    if (entry_offset == -1) {
        printf("err: entry point not found\n");
        return 0;
    }

    /*
     * perf numbers
     */
    if (program_args.perf)
        printf("pyvscc: compiled into binary in %ld us\n", end_time - start_time);

    /*
     * map bytecode into executable memory and execute
     */
    void *mapped = map(&ctx.compiled_data);
    entry_point_fnptr entry = mapped + entry_offset;

    /*
     * execution
     */
    start_time = time_us();
    entry();
    end_time = time_us();

    /*
     * perf numbers
     */
    if (program_args.perf)
        printf("pyvscc: executed for %ld us\n", end_time - start_time);

    /*
     * free mapped memory
     */
    munmap(mapped, ctx.compiled_data.length);
}