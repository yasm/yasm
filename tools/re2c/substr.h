#ifndef re2c_substr_h
#define re2c_substr_h

#include <stdio.h>
#include <stdlib.h>
#include "basics.h"

struct SubStr {
    char		*str;
    uint		len;
};

typedef struct SubStr SubStr;

int SubStr_eq(const SubStr *, const SubStr *);
static inline SubStr *SubStr_new_u(uchar*, uint);
static inline SubStr *SubStr_new(char*, uint);
static inline SubStr *SubStr_copy(const SubStr*);
void SubStr_out(const SubStr *, FILE *);
#define SubStr_delete(x)    free(x)

typedef struct SubStr Str;

Str *Str_new(const SubStr*);
Str *Str_copy(Str*);
Str *Str_new_empty(void);
void Str_delete(Str *);

static inline SubStr *
SubStr_new_u(uchar *s, uint l)
{
    SubStr *r = malloc(sizeof(SubStr));
    r->str = (char*)s;
    r->len = l;
    return r;
}

static inline SubStr *
SubStr_new(char *s, uint l)
{
    SubStr *r = malloc(sizeof(SubStr));
    r->str = s;
    r->len = l;
    return r;
}

static inline SubStr *
SubStr_copy(const SubStr *s)
{
    SubStr *r = malloc(sizeof(SubStr));
    r->str = s->str;
    r->len = s->len;
    return r;
}

#endif
