#ifndef _substr_h
#define _substr_h

#include <iostream.h>
#include "basics.h"

class SubStr {
public:
    char		*str;
    uint		len;
public:
    friend bool operator==(const SubStr &, const SubStr &);
    SubStr(uchar*, uint);
    SubStr(char*, uint);
    SubStr(const SubStr&);
    void out(ostream&) const;
};

class Str: public SubStr {
public:
    Str(const SubStr&);
    Str(Str&);
    Str();
    ~Str();
};

inline ostream& operator<<(ostream& o, const SubStr &s){
    s.out(o);
    return o;
}

inline ostream& operator<<(ostream& o, const SubStr* s){
    return o << *s;
}

inline SubStr::SubStr(uchar *s, uint l)
    : str((char*) s), len(l) { }

inline SubStr::SubStr(char *s, uint l)
    : str(s), len(l) { }

inline SubStr::SubStr(const SubStr &s)
    : str(s.str), len(s.len) { }

#endif
