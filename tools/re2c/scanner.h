#ifndef _scanner_h
#define	_scanner_h

#include <stdio.h>
#include "token.h"

typedef struct Scanner {
    FILE		*in;
    uchar		*bot, *tok, *ptr, *cur, *pos, *lim, *top, *eof;
    uint		tchar, tline, cline;
} Scanner;

void Scanner_init(Scanner*, FILE *);
static inline Scanner *Scanner_new(FILE *);

int Scanner_echo(Scanner*, FILE *);
int Scanner_scan(Scanner*);
void Scanner_fatal(Scanner*, char*);
SubStr Scanner_token(Scanner*);
static inline uint Scanner_line(Scanner*);

inline SubStr
Scanner_token(Scanner *s)
{
    SubStr r;
    SubStr_init_u(&r, s->tok, s->cur - s->tok);
    return r;
}

static inline uint
Scanner_line(Scanner *s)
{
    return s->cline;
}

static inline Scanner *
Scanner_new(FILE *i)
{
    Scanner *r = malloc(sizeof(Scanner));
    Scanner_init(r, i);
    return r;
}

#endif
