#ifndef _token_h
#define	_token_h

#include "substr.h"

class Token {
  public:
    Str			text;
    uint		line;
  public:
    Token(SubStr, uint);
};

inline Token::Token(SubStr t, uint l) : text(t), line(l) {
    ;
}

#endif
