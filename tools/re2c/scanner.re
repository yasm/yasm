#include <stdlib.h>
#include <string.h>
#include "scanner.h"
#include "parse.h"
#include "parser.h"

extern YYSTYPE yylval;

#define	BSIZE	8192

#define	YYCTYPE		uchar
#define	YYCURSOR	cursor
#define	YYLIMIT		s->lim
#define	YYMARKER	s->ptr
#define	YYFILL(n)	{cursor = fill(s, cursor);}

#define	RETURN(i)	{s->cur = cursor; return i;}

static uchar *fill(Scanner*, uchar*);

void
Scanner_init(Scanner *s, FILE *i)
{
    s->in = i;
    s->bot = s->tok = s->ptr = s->cur = s->pos = s->lim = s->top =
	     s->eof = NULL;
    s->tchar = s->tline = 0;
    s->cline = 1;
}

static uchar *
fill(Scanner *s, uchar *cursor)
{
    if(!s->eof){
	uint cnt = s->tok - s->bot;
	if(cnt){
	    memcpy(s->bot, s->tok, s->lim - s->tok);
	    s->tok = s->bot;
	    s->ptr -= cnt;
	    cursor -= cnt;
	    s->pos -= cnt;
	    s->lim -= cnt;
	}
	if((s->top - s->lim) < BSIZE){
	    uchar *buf = malloc(sizeof(uchar)*((s->lim - s->bot) + BSIZE));
	    memcpy(buf, s->tok, s->lim - s->tok);
	    s->tok = buf;
	    s->ptr = &buf[s->ptr - s->bot];
	    cursor = &buf[cursor - s->bot];
	    s->pos = &buf[s->pos - s->bot];
	    s->lim = &buf[s->lim - s->bot];
	    s->top = &s->lim[BSIZE];
	    free(s->bot);
	    s->bot = buf;
	}
	if((cnt = fread(s->lim, sizeof(uchar), BSIZE, s->in)) != BSIZE){
	    s->eof = &s->lim[cnt]; *s->eof++ = '\n';
	}
	s->lim += cnt;
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

int
Scanner_echo(Scanner *s, FILE *out)
{
    uchar *cursor = s->cur;
    s->tok = cursor;
echo:
/*!re2c
	"/*!re2c"		{ fwrite(s->tok, 1, &cursor[-7] - s->tok, out);
				  s->tok = cursor;
				  RETURN(1); }
	"\n"			{ if(cursor == s->eof) RETURN(0);
				  fwrite(s->tok, 1, cursor - s->tok, out);
				  s->tok = s->pos = cursor; s->cline++;
				  goto echo; }
        any			{ goto echo; }
*/
}


int
Scanner_scan(Scanner *s)
{
    uchar *cursor = s->cur;
    uint depth;

scan:
    s->tchar = cursor - s->pos;
    s->tline = s->cline;
    s->tok = cursor;
/*!re2c
	"{"			{ depth = 1;
				  goto code;
				}
	"/*"			{ depth = 1;
				  goto comment; }

	"*/"			{ s->tok = cursor;
				  RETURN(0); }

	dstring			{ s->cur = cursor;
				  yylval.regexp = strToRE(Scanner_token(s));
				  return STRING; }
	"\""			{ Scanner_fatal(s, "bad string"); }

	cstring			{ s->cur = cursor;
				  yylval.regexp = ranToRE(Scanner_token(s));
				  return RANGE; }
	"["			{ Scanner_fatal(s, "bad character constant"); }

	[()|=;/\\]		{ RETURN(*s->tok); }

	[*+?]			{ yylval.op = *s->tok;
				  RETURN(CLOSE); }

	letter (letter|digit)*	{ SubStr substr;
				  s->cur = cursor;
				  substr = Scanner_token(s);
				  yylval.symbol = Symbol_find(&substr);
				  return ID; }

	[ \t]+			{ goto scan; }

	"\n"			{ if(cursor == s->eof) RETURN(0);
				  s->pos = cursor; s->cline++;
				  goto scan;
	    			}

	any			{ fprintf(stderr, "unexpected character: '%c'\n", *s->tok);
				  goto scan;
				}
*/

code:
/*!re2c
	"}"			{ if(--depth == 0){
					s->cur = cursor;
					yylval.token = Token_new(Scanner_token(s), s->tline);
					return CODE;
				  }
				  goto code; }
	"{"			{ ++depth;
				  goto code; }
	"\n"			{ if(cursor == s->eof) Scanner_fatal(s, "missing '}'");
				  s->pos = cursor; s->cline++;
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
	"\n"			{ if(cursor == s->eof) RETURN(0);
				  s->tok = s->pos = cursor; s->cline++;
				  goto comment;
				}
        any			{ goto comment; }
*/
}

void
Scanner_fatal(Scanner *s, char *msg)
{
    fprintf(stderr, "line %d, column %d: %s\n", s->tline, s->tchar + 1, msg);
    exit(1);
}
