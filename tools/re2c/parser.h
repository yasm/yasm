#ifndef _parser_h
#define _parser_h

#include "scanner.h"
#include "re.h"

class Symbol {
public:
    static Symbol	*first;
    Symbol		*next;
    Str			name;
    RegExp		*re;
public:
    Symbol(const SubStr&);
    static Symbol *find(const SubStr&);
};

void parse(int, ostream&);

#endif
