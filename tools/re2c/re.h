#ifndef _re_h
#define _re_h

#include <iostream.h>
#include "token.h"
#include "ins.h"

struct CharPtn {
    uint	card;
    CharPtn	*fix;
    CharPtn	*nxt;
};

struct CharSet {
    CharPtn	*fix;
    CharPtn	*freeHead, **freeTail;
    CharPtn	*rep[nChars];
    CharPtn	ptn[nChars];
};

class Range {
public:
    Range	*next;
    uint	lb, ub;		// [lb,ub)
public:
    Range(uint l, uint u) : next(NULL), lb(l), ub(u)
	{ }
    Range(Range &r) : next(NULL), lb(r.lb), ub(r.ub)
	{ }
    friend ostream& operator<<(ostream&, const Range&);
    friend ostream& operator<<(ostream&, const Range*);
};

inline ostream& operator<<(ostream &o, const Range *r){
	return r? o << *r : o;
}

class RegExp {
public:
    uint	size;
public:
    virtual char *typeOf() = 0;
    RegExp *isA(char *t)
	{ return typeOf() == t? this : NULL; }
    virtual void split(CharSet&) = 0;
    virtual void calcSize(Char*) = 0;
    virtual uint fixedLength();
    virtual void compile(Char*, Ins*) = 0;
    virtual void display(ostream&) const = 0;
    friend ostream& operator<<(ostream&, const RegExp&);
    friend ostream& operator<<(ostream&, const RegExp*);
};

inline ostream& operator<<(ostream &o, const RegExp &re){
    re.display(o);
    return o;
}

inline ostream& operator<<(ostream &o, const RegExp *re){
    return o << *re;
}

class NullOp: public RegExp {
public:
    static char *type;
public:
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    uint fixedLength();
    void compile(Char*, Ins*);
    void display(ostream &o) const {
	o << "_";
    }
};

class MatchOp: public RegExp {
public:
    static char *type;
    Range	*match;
public:
    MatchOp(Range *m) : match(m)
	{ }
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    uint fixedLength();
    void compile(Char*, Ins*);
    void display(ostream&) const;
};

class RuleOp: public RegExp {
private:
    RegExp	*exp;
public:
    RegExp	*ctx;
    static char *type;
    Ins		*ins;
    uint	accept;
    Token	*code;
    uint	line;
public:
    RuleOp(RegExp*, RegExp*, Token*, uint);
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    void compile(Char*, Ins*);
    void display(ostream &o) const {
	o << exp << "/" << ctx << ";";
    }
};

class AltOp: public RegExp {
private:
    RegExp	*exp1, *exp2;
public:
    static char *type;
public:
    AltOp(RegExp *e1, RegExp *e2)
	{ exp1 = e1;  exp2 = e2; }
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    uint fixedLength();
    void compile(Char*, Ins*);
    void display(ostream &o) const {
	o << exp1 << "|" << exp2;
    }
    friend RegExp *mkAlt(RegExp*, RegExp*);
};

class CatOp: public RegExp {
private:
    RegExp	*exp1, *exp2;
public:
    static char *type;
public:
    CatOp(RegExp *e1, RegExp *e2)
	{ exp1 = e1;  exp2 = e2; }
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    uint fixedLength();
    void compile(Char*, Ins*);
    void display(ostream &o) const {
	o << exp1 << exp2;
    }
};

class CloseOp: public RegExp {
private:
    RegExp	*exp;
public:
    static char *type;
public:
    CloseOp(RegExp *e)
	{ exp = e; }
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    void compile(Char*, Ins*);
    void display(ostream &o) const {
	o << exp << "+";
    }
};

extern void genCode(ostream&, RegExp*);
extern RegExp *mkDiff(RegExp*, RegExp*);
extern RegExp *strToRE(SubStr);
extern RegExp *ranToRE(SubStr);

#endif
