#include <string.h>
#include <vscc.h>
#include "asm/assembler.h"
#include "ir/fmt.h"
#include "ir/intermediate.h"
#include "lexer.h"
#include "pyimpl.h"
#include "util.h"
#include "pybuild.h"

#include <stdio.h>
#include <sys/mman.h>

#define LOG_DEFAULT "\033[0m"
#define LOG_RED "\033[0;31m"
#define LOG_GREEN "\033[0;32m"
#define LOG_YELLOW "\033[0;33m"
#define LOG_CYAN "\033[0;36m"

typedef uint64_t(*entry_point_fnptr)();

static const char *usage = 
    "usage: pyvscc [-h] [-i FILE_PATH] [-e ENTRY_POINT] [-m SIZE] [-s SIZE] [-u]\n\n"
    "options:\n"
    "  -h:                  display help information\n"
    "  -i [FILE_PATH]:      input file path\n"
    "  -e [ENTRY_POINT]:    specify entry function (if not specified, searches for any function containing 'main')\n"
    "  -m [SIZE]:           max amount of bytes program may allocate (default: 4098 bytes)"
    "  -s [SIZE]:           amount of bytes variables/functions with an unspecified type take up (default: 8 bytes)\n"
    "  -u                   unsafe flag, ignores errors, executes first function if main not specified/found\n";

struct args {
    char *filepath;
    char *entry;
    size_t default_size;
    size_t max_mem;
    bool unsafe;
};

static void *map(struct vscc_compiled_data *compiled)
{
    void *exe = mmap(NULL, compiled->length, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    memcpy(exe, compiled->buffer, compiled->length);
    return exe;
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
        .unsafe = false
    };

    for (int i = 1; i < argc; i += 2) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'h':
                printf("%s", usage);
                return 0;
            case 'i':
                program_args.filepath = argv[i + 1];
                break;
            case 'e':
                program_args.entry = argv[i + 1];
                break;
            case 'm':
                program_args.max_mem = atoi(argv[i + 1]);
                break;
            case 's':
                program_args.default_size = atoi(argv[i + 1]);
                break;
            case 'u':
                program_args.unsafe = true;
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

        .current_label = 100,
        .labelc = 0,

        .current_function = NULL,
        .default_size = program_args.default_size,

        .memcpy_queue = NULL
    };

    /*
     * append python functions & environmental variables
     */
    pyimpl_append_to_context(&ctx.vscc_ctx);

    /*
     * parse file and construct intermediate representation
     */
    bool status = parse(&ctx, tokens);
    if (!status && !program_args.unsafe) {
        printf("err: failed to compile\n");
        return 0;
    }

    /*
     * compile code and obtain entry point offset
     */
    uintptr_t entry_offset = build(&ctx);
    if (entry_offset == -1) {
        if (!program_args.unsafe) {
            printf("err: entry point not found\n");
            return 0;
        } 
        else {
            entry_offset = 0;
        }
    }

    /*
     * map bytecode into executable memory and execute
     */
    void *mapped = map(&ctx.compiled_data);
    entry_point_fnptr entry = mapped + entry_offset;
    entry();

    /*
     * free mapped memory
     */
    munmap(mapped, ctx.compiled_data.length);
}