#ifndef re2c_ins_h
#define re2c_ins_h

#include "basics.h"

#define nChars 256
typedef uchar Char;

#define CHAR 0
#define GOTO 1
#define FORK 2
#define TERM 3
#define CTXT 4

typedef union Ins {
    struct {
	byte	tag;
	byte	marked;
	void	*link;
    }			i;
    struct {
	ushort	value;
	ushort	bump;
	void	*link;
    }			c;
} Ins;

static inline int isMarked(Ins *i){
    return i->i.marked != 0;
}

static inline void mark(Ins *i){
    i->i.marked = 1;
}

static inline void unmark(Ins *i){
    i->i.marked = 0;
}

#endif
