#include "lexer.h"
#include <vscc.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* START: https://stackoverflow.com/a/12546318 */

// str_replace(haystack, haystacksize, oldneedle, newneedle) --
//  Search haystack and replace all occurences of oldneedle with newneedle.
//  Resulting haystack contains no more than haystacksize characters (including the '\0').
//  If haystacksize is too small to make the replacements, do not modify haystack at all.
//
// RETURN VALUES
// str_replace() returns haystack on success and NULL on failure. 
// Failure means there was not enough room to replace all occurences of oldneedle.
// Success is returned otherwise, even if no replacement is made.
char *str_replace(char *haystack, size_t haystacksize,
                    const char *oldneedle, const char *newneedle);

// ------------------------------------------------------------------
// Implementation of function
// ------------------------------------------------------------------
#define SUCCESS (char *)haystack
#define FAILURE (void *)NULL

static bool
locate_forward(char **needle_ptr, char *read_ptr, 
        const char *needle, const char *needle_last);
static bool
locate_backward(char **needle_ptr, char *read_ptr, 
        const char *needle, const char *needle_last);

char *str_replace(char *haystack, size_t haystacksize,
                    const char *oldneedle, const char *newneedle)
{   
    size_t oldneedle_len = strlen(oldneedle);
    size_t newneedle_len = strlen(newneedle);
    char *oldneedle_ptr;    // locates occurences of oldneedle
    char *read_ptr;         // where to read in the haystack
    char *write_ptr;        // where to write in the haystack
    const char *oldneedle_last =  // the last character in oldneedle
        oldneedle +             
        oldneedle_len - 1;      

    // Case 0: oldneedle is empty
    if (oldneedle_len == 0)
        return SUCCESS;     // nothing to do; define as success

    // Case 1: newneedle is not longer than oldneedle
    if (newneedle_len <= oldneedle_len) {       
        // Pass 1: Perform copy/replace using read_ptr and write_ptr
        for (oldneedle_ptr = (char *)oldneedle,
            read_ptr = haystack, write_ptr = haystack; 
            *read_ptr != '\0';
            read_ptr++, write_ptr++)
        {
            *write_ptr = *read_ptr;         
            bool found = locate_forward(&oldneedle_ptr, read_ptr,
                        oldneedle, oldneedle_last);
            if (found)  {   
                // then perform update
                write_ptr -= oldneedle_len;
                memcpy(write_ptr+1, newneedle, newneedle_len);
                write_ptr += newneedle_len;
            }               
        } 
        *write_ptr = '\0';
        return SUCCESS;
    }

    // Case 2: newneedle is longer than oldneedle
    else {
        size_t diff_len =       // the amount of extra space needed 
            newneedle_len -     // to replace oldneedle with newneedle
            oldneedle_len;      // in the expanded haystack

        // Pass 1: Perform forward scan, updating write_ptr along the way
        for (oldneedle_ptr = (char *)oldneedle,
            read_ptr = haystack, write_ptr = haystack;
            *read_ptr != '\0';
            read_ptr++, write_ptr++)
        {
            bool found = locate_forward(&oldneedle_ptr, read_ptr, 
                        oldneedle, oldneedle_last);
            if (found) {    
                // then advance write_ptr
                write_ptr += diff_len;
            }
            if (write_ptr >= haystack+haystacksize)
                return FAILURE; // no more room in haystack
        }

        // Pass 2: Walk backwards through haystack, performing copy/replace
        for (oldneedle_ptr = (char *)oldneedle_last;
            write_ptr >= haystack;
            write_ptr--, read_ptr--)
        {
            *write_ptr = *read_ptr;
            bool found = locate_backward(&oldneedle_ptr, read_ptr, 
                        oldneedle, oldneedle_last);
            if (found) {    
                // then perform replacement
                write_ptr -= diff_len;
                memcpy(write_ptr, newneedle, newneedle_len);
            }   
        }
        return SUCCESS;
    }
}

// locate_forward: compare needle_ptr and read_ptr to see if a match occured
// needle_ptr is updated as appropriate for the next call
// return true if match occured, false otherwise
static inline bool 
locate_forward(char **needle_ptr, char *read_ptr,
        const char *needle, const char *needle_last)
{
    if (**needle_ptr == *read_ptr) {
        (*needle_ptr)++;
        if (*needle_ptr > needle_last) {
            *needle_ptr = (char *)needle;
            return true;
        }
    }
    else 
        *needle_ptr = (char *)needle;
    return false;
}

// locate_backward: compare needle_ptr and read_ptr to see if a match occured
// needle_ptr is updated as appropriate for the next call
// return true if match occured, false otherwise
static inline bool
locate_backward(char **needle_ptr, char *read_ptr, 
        const char *needle, const char *needle_last)
{
    if (**needle_ptr == *read_ptr) {
        (*needle_ptr)--;
        if (*needle_ptr < needle) {
            *needle_ptr = (char *)needle_last;
            return true;
        }
    }
    else 
        *needle_ptr = (char *)needle_last;
    return false;
}

/* END */

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
    
    /* fix strings */
    for (struct lexer_token *token = root; token; token = token->next) {
        if (token->type == TOKEN_STRING) {
            str_replace(token->contents, 64, "\\n", "\n");
            str_replace(token->contents, 64, "\\t", "\t");
        }
    }

    return root;
}