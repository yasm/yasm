#ifndef _scanner_h
#define	_scanner_h

#include "token.h"

class Scanner {
  private:
    int			in;
    uchar		*bot, *tok, *ptr, *cur, *pos, *lim, *top, *eof;
    uint		tchar, tline, cline;
  private:
    uchar *fill(uchar*);
  public:
    Scanner(int);
    int echo(ostream&);
    int scan();
    void fatal(char*);
    SubStr token();
    uint line();
};

inline SubStr Scanner::token(){
    return SubStr(tok, cur - tok);
}

inline uint Scanner::line(){
    return cline;
}

#endif
