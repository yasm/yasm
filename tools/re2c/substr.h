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

static inline void SubStr_init_u(SubStr*, uchar*, uint);
static inline SubStr *SubStr_new_u(uchar*, uint);

static inline void SubStr_init(SubStr*, char*, uint);
static inline SubStr *SubStr_new(char*, uint);

static inline void SubStr_copy(SubStr*, const SubStr*);
static inline SubStr *SubStr_new_copy(const SubStr*);

void SubStr_out(const SubStr*, FILE *);
#define SubStr_delete(x)    free(x)

typedef struct SubStr Str;

void Str_init(Str*, const SubStr*);
Str *Str_new(const SubStr*);

void Str_copy(Str*, Str*);
Str *Str_new_copy(Str*);

Str *Str_new_empty(void);
void Str_destroy(Str *);
void Str_delete(Str *);

static inline void
SubStr_init_u(SubStr *r, uchar *s, uint l)
{
    r->str = (char*)s;
    r->len = l;
}

static inline SubStr *
SubStr_new_u(uchar *s, uint l)
{
    SubStr *r = malloc(sizeof(SubStr));
    r->str = (char*)s;
    r->len = l;
    return r;
}

static inline void
SubStr_init(SubStr *r, char *s, uint l)
{
    r->str = s;
    r->len = l;
}

static inline SubStr *
SubStr_new(char *s, uint l)
{
    SubStr *r = malloc(sizeof(SubStr));
    r->str = s;
    r->len = l;
    return r;
}

static inline void
SubStr_copy(SubStr *r, const SubStr *s)
{
    r->str = s->str;
    r->len = s->len;
}

static inline SubStr *
SubStr_new_copy(const SubStr *s)
{
    SubStr *r = malloc(sizeof(SubStr));
    r->str = s->str;
    r->len = s->len;
    return r;
}

#endif
