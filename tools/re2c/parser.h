#ifndef re2c_parser_h
#define re2c_parser_h

#include <stdio.h>
#include "scanner.h"
#include "re.h"

typedef struct Symbol {
    struct Symbol		*next;
    Str			name;
    RegExp		*re;
} Symbol;

void Symbol_init(Symbol *, const SubStr*);
static inline Symbol *Symbol_new(const SubStr*);
Symbol *Symbol_find(const SubStr*);

void parse(FILE *, FILE *);

static inline Symbol *
Symbol_new(const SubStr *str)
{
    Symbol *r = malloc(sizeof(Symbol));
    Symbol_init(r, str);
    return r;
}

#endif
