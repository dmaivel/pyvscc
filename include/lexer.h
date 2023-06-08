#ifndef _LEXER_H_
#define _LEXER_H_

enum lexer_token_type {
    TOKEN_NONE,
    TOKEN_OPEN_BRACE,
    TOKEN_CLOSE_BRACE,
    TOKEN_OPEN_PAREN,
    TOKEN_CLOSE_PAREN,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_TYPE_INT,
    TOKEN_RETURN,
    TOKEN_IDENTIFIER,
    TOKEN_LITERAL,
    TOKEN_QUOTE,
    TOKEN_STRING,
    TOKEN_IF,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_DEF,
    TOKEN_COMMA,

    TOKEN_EQUAL,
    TOKEN_ADDEQ,
    TOKEN_SUBEQ,
    TOKEN_MULEQ,
    TOKEN_DIVEQ,
    TOKEN_RARROW,
    TOKEN_COMMENT,

    TOKEN_EQUALS,
    TOKEN_NEQUALS,
    TOKEN_LESSTHAN,
    TOKEN_GREATERTHAN,

    TOKEN_WHITESPACE,
    TOKEN_NEWLINE,
    TOKEN_TAB
};

struct lexer_token {
    enum lexer_token_type type;
    char contents[64];

    struct lexer_token *next;
};

struct lexer_token *str_to_tokens(const char *buffer);

#endif /* _LEXER_H_ */