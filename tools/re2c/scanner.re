#include <stdlib.h>
#include <string.h>
#include <iostream.h>
#include <unistd.h>
#include "scanner.h"
#include "parser.h"
#include "y.tab.h"

extern YYSTYPE yylval;

#define	BSIZE	8192

#define	YYCTYPE		uchar
#define	YYCURSOR	cursor
#define	YYLIMIT		lim
#define	YYMARKER	ptr
#define	YYFILL(n)	{cursor = fill(cursor);}

#define	RETURN(i)	{cur = cursor; return i;}


Scanner::Scanner(int i) : in(i),
	bot(NULL), tok(NULL), ptr(NULL), cur(NULL), pos(NULL), lim(NULL),
	top(NULL), eof(NULL), tchar(0), tline(0), cline(1) {
    ;
}

uchar *Scanner::fill(uchar *cursor){
    if(!eof){
	uint cnt = tok - bot;
	if(cnt){
	    memcpy(bot, tok, lim - tok);
	    tok = bot;
	    ptr -= cnt;
	    cursor -= cnt;
	    pos -= cnt;
	    lim -= cnt;
	}
	if((top - lim) < BSIZE){
	    uchar *buf = new uchar[(lim - bot) + BSIZE];
	    memcpy(buf, tok, lim - tok);
	    tok = buf;
	    ptr = &buf[ptr - bot];
	    cursor = &buf[cursor - bot];
	    pos = &buf[pos - bot];
	    lim = &buf[lim - bot];
	    top = &lim[BSIZE];
	    delete [] bot;
	    bot = buf;
	}
	if((cnt = read(in, (char*) lim, BSIZE)) != BSIZE){
	    eof = &lim[cnt]; *eof++ = '\n';
	}
	lim += cnt;
    }
    return cursor;
}

/*!re2c
any		= [\000-\377];
dot		= any \ [\n];
esc		= dot \ [\\];
cstring		= "["  ((esc \ [\]]) | "\\" dot)* "]" ;
dstring		= "\"" ((esc \ ["] ) | "\\" dot)* "\"";
sstring		= "'"  ((esc \ ['] ) | "\\" dot)* "'" ;
letter		= [a-zA-Z];
digit		= [0-9];
*/

int Scanner::echo(ostream &out){
    uchar *cursor = cur;
    tok = cursor;
echo:
/*!re2c
	"/*!re2c"		{ out.write(tok, &cursor[-7] - tok);
				  tok = cursor;
				  RETURN(1); }
	"\n"			{ if(cursor == eof) RETURN(0);
				  out.write(tok, cursor - tok);
				  tok = pos = cursor; cline++;
				  goto echo; }
        any			{ goto echo; }
*/
}


int Scanner::scan(){
    uchar *cursor = cur;
    uint depth;

scan:
    tchar = cursor - pos;
    tline = cline;
    tok = cursor;
/*!re2c
	"{"			{ depth = 1;
				  goto code;
				}
	"/*"			{ depth = 1;
				  goto comment; }

	"*/"			{ tok = cursor;
				  RETURN(0); }

	dstring			{ cur = cursor;
				  yylval.regexp = strToRE(token());
				  return STRING; }
	"\""			{ fatal("bad string"); }

	cstring			{ cur = cursor;
				  yylval.regexp = ranToRE(token());
				  return RANGE; }
	"["			{ fatal("bad character constant"); }

	[()|=;/\\]		{ RETURN(*tok); }

	[*+?]			{ yylval.op = *tok;
				  RETURN(CLOSE); }

	letter (letter|digit)*	{ cur = cursor;
				  yylval.symbol = Symbol::find(token());
				  return ID; }

	[ \t]+			{ goto scan; }

	"\n"			{ if(cursor == eof) RETURN(0);
				  pos = cursor; cline++;
				  goto scan;
	    			}

	any			{ cerr << "unexpected character: " << *tok << endl;
				  goto scan;
				}
*/

code:
/*!re2c
	"}"			{ if(--depth == 0){
					cur = cursor;
					yylval.token = new Token(token(), tline);
					return CODE;
				  }
				  goto code; }
	"{"			{ ++depth;
				  goto code; }
	"\n"			{ if(cursor == eof) fatal("missing '}'");
				  pos = cursor; cline++;
				  goto code;
				}
	dstring | sstring | any	{ goto code; }
*/

comment:
/*!re2c
	"*/"			{ if(--depth == 0)
					goto scan;
				    else
					goto comment; }
	"/*"			{ ++depth;
				  goto comment; }
	"\n"			{ if(cursor == eof) RETURN(0);
				  tok = pos = cursor; cline++;
				  goto comment;
				}
        any			{ goto comment; }
*/
}

void Scanner::fatal(char *msg){
    cerr << "line " << tline << ", column " << (tchar + 1) << ": "
	<< msg << endl;
    exit(1);
}
