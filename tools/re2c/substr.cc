#include <string.h>
#include "substr.h"

void SubStr::out(ostream& o) const {
    o.write(str, len);
}

bool operator==(const SubStr &s1, const SubStr &s2){
    return (bool) (s1.len == s2.len && memcmp(s1.str, s2.str, s1.len) == 0);
}

Str::Str(const SubStr& s) : SubStr(new char[s.len], s.len) {
    memcpy(str, s.str, s.len);
}

Str::Str(Str& s) : SubStr(s.str, s.len) {
    s.str = NULL;
    s.len = 0;
}

Str::Str() : SubStr((char*) NULL, 0) {
    ;
}


Str::~Str() {
    delete str;
    str = (char*)-1;
    len = (uint)-1;
}
