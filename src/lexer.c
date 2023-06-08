#include "lexer.h"
#include <vscc.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static struct lexer_token *add_token(struct lexer_token **root, struct lexer_token **last, enum lexer_token_type type, char *contents)
{
    if (unlikely(*last == NULL)) {
        *root = calloc(1, sizeof(struct lexer_token));
        *last = *root;
    }
    else {
        (*last)->next = calloc(1, sizeof(struct lexer_token));
        (*last) = (*last)->next;
    }

    (*last)->type = type;
    strcpy((*last)->contents, contents);

    return *last;
}

static enum lexer_token_type get_token_type_c(char c)
{
    /* small table for "special characters" */
    static const struct {
        char c;
        enum lexer_token_type type;
    } lookup_table[] = {
        { .c = '{', .type = TOKEN_OPEN_BRACE },
        { .c = '}', .type = TOKEN_CLOSE_BRACE },
        { .c = '(', .type = TOKEN_OPEN_PAREN },
        { .c = ')', .type = TOKEN_CLOSE_PAREN },
        { .c = ';', .type = TOKEN_SEMICOLON },
        { .c = ':', .type = TOKEN_COLON },
        { .c = '#', .type = TOKEN_COMMENT },
        { .c = '\"', .type = TOKEN_QUOTE },
        { .c = '\'', .type = TOKEN_QUOTE },
        { .c = ',', .type = TOKEN_COMMA },

        { .c = ' ', .type = TOKEN_WHITESPACE },
        { .c = '\n', .type = TOKEN_NEWLINE },
        { .c = '\t', .type = TOKEN_TAB },
    };

    /* check if regular character */
    if (isalpha(c) || c == '_')
        return TOKEN_IDENTIFIER;
    if (isdigit(c))
        return TOKEN_LITERAL;

    /* check if "special character" */
    for (int i = 0; i < sizeof(lookup_table) / sizeof(*lookup_table); i++) {
        if (c == lookup_table[i].c)
            return lookup_table[i].type;
    }

    /* else */
    return TOKEN_NONE;
} 

struct lexer_token *str_to_tokens(const char *buffer)
{
    struct lexer_token *root = NULL;
    struct lexer_token *last = NULL;

    static const struct {
        char cmp[16];
        enum lexer_token_type new_type;
    } identifier_to_new_type[] = {
        { .cmp = "int",     .new_type = TOKEN_TYPE_INT },
        { .cmp = "return",  .new_type = TOKEN_RETURN },
        { .cmp = "if",      .new_type = TOKEN_IF },
        { .cmp = "while",   .new_type = TOKEN_WHILE },
        { .cmp = "for",     .new_type = TOKEN_FOR },
        { .cmp = "def",     .new_type = TOKEN_DEF },
    };

    static const struct {
        char operator[4];
        enum lexer_token_type new_type;
    } operator_to_type[] = {
        { .operator = "=",     .new_type = TOKEN_EQUAL },
        { .operator = "+=",    .new_type = TOKEN_ADDEQ },
        { .operator = "-=",    .new_type = TOKEN_SUBEQ },
        { .operator = "->",    .new_type = TOKEN_RARROW },

        { .operator = "==",    .new_type = TOKEN_EQUALS },
        { .operator = "!=",    .new_type = TOKEN_NEQUALS },
    };

    enum lexer_token_type current_token_type = get_token_type_c(*buffer);
    add_token(&root, &last, current_token_type, "");
    int i = 0;

    bool in_string = false;

    for (char c = *buffer; c; c = *++buffer) {
        enum lexer_token_type current_type = get_token_type_c(*buffer);

        /* begin new token */
        if (current_token_type != current_type || current_token_type == TOKEN_TAB) {
            /* initial string check */
            if (in_string && current_type != TOKEN_QUOTE) {
                if (c == '\\') {
                    switch (*(buffer + 1)) {
                    case 'n':
                        last->contents[i++] = '\n';
                        break;
                    default:
                        last->contents[i++] = c;
                        continue;
                    }

                    buffer++;
                    continue;
                }

                last->contents[i++] = c;
                continue;
            }

            /* check if identifier can be converted into more accurate type */
            if (current_token_type == TOKEN_IDENTIFIER) {
                for (int i = 0; i < sizeof(identifier_to_new_type) / sizeof(*identifier_to_new_type); i++) {
                    if (strcmp(last->contents, identifier_to_new_type[i].cmp) == 0) {
                        last->type = identifier_to_new_type[i].new_type;
                        break;
                    }
                }
            }

            /* check if string started */
            if (current_type == TOKEN_QUOTE) {
                if (!in_string) {
                    current_type = TOKEN_IDENTIFIER;
                    in_string = true;
                }
                else {
                    last->type = TOKEN_STRING;
                    in_string = false;
                    last->contents[i++] = c;
                    continue;
                }
            }

            /* check if token is none to fix */
            if (current_token_type == TOKEN_NONE) {
                for (int i = 0; i < sizeof(operator_to_type) / sizeof(*operator_to_type); i++) {
                    if (strcmp(last->contents, operator_to_type[i].operator) == 0) {
                        last->type = operator_to_type[i].new_type;
                        break;
                    }
                }
            }

            add_token(&root, &last, current_type, "");
            current_token_type = current_type;
            last->contents[0] = c;
            i = 1;
            continue;
        } 

        /* otherwise continue onto current token */
        else {
            assert(i < 64);
            last->contents[i++] = c;
        }
    }    

    return root;
}