#ifndef re2c_token_h
#define	re2c_token_h

#include "substr.h"

typedef struct Token {
    Str			text;
    uint		line;
} Token;

static inline void Token_init(Token *, SubStr, uint);
static inline Token *Token_new(SubStr, uint);

static inline void
Token_init(Token *r, SubStr t, uint l)
{
    Str_copy(&r->text, &t);
    r->line = l;
}

static inline Token *
Token_new(SubStr t, uint l)
{
    Token *r = malloc(sizeof(Token));
    Str_init(&r->text, &t);
    r->line = l;
    return r;
}

#endif
