/*
 * NASM-compatible lex lexer
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 *  Portions based on re2c's example code.
 *
 *  This file is part of YASM.
 *
 *  YASM is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  YASM is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "util.h"
RCSID("$IdPath$");

#include "bitvect.h"

#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

#include "bytecode.h"

#include "arch.h"

#include "src/parsers/nasm/nasm-defs.h"
#include "nasm-bison.h"


#define BSIZE	8192

#define YYCTYPE		char
#define YYCURSOR	cursor
#define YYLIMIT		s.lim
#define YYMARKER	s.ptr
#define YYFILL(n)	{cursor = fill(cursor);}

#define RETURN(i)	{s.cur = cursor; return i;}

#define SCANINIT()	{ \
	s.tchar = cursor - s.pos; \
	s.tline = s.cline; \
	s.tok = cursor; \
    }

#define TOKLEN		(cursor-s.tok)

void nasm_parser_cleanup(void);
void nasm_parser_set_directive_state(void);
int nasm_parser_lex(void);

extern size_t (*nasm_parser_input) (char *buf, size_t max_size);


typedef struct Scanner {
    YYCTYPE		*bot, *tok, *ptr, *cur, *pos, *lim, *top, *eof;
    unsigned int	tchar, tline, cline;
} Scanner;

static Scanner s = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 1 };

FILE *nasm_parser_in = NULL;

static YYCTYPE *
fill(YYCTYPE *cursor)
{
    if(!s.eof){
	size_t cnt = s.tok - s.bot;
	if(cnt){
	    memcpy(s.bot, s.tok, s.lim - s.tok);
	    s.tok = s.bot;
	    s.ptr -= cnt;
	    cursor -= cnt;
	    s.pos -= cnt;
	    s.lim -= cnt;
	}
	if((s.top - s.lim) < BSIZE){
	    char *buf = xmalloc((s.lim - s.bot) + BSIZE);
	    memcpy(buf, s.tok, s.lim - s.tok);
	    s.tok = buf;
	    s.ptr = &buf[s.ptr - s.bot];
	    cursor = &buf[cursor - s.bot];
	    s.pos = &buf[s.pos - s.bot];
	    s.lim = &buf[s.lim - s.bot];
	    s.top = &s.lim[BSIZE];
	    if (s.bot)
		xfree(s.bot);
	    s.bot = buf;
	}
	if((cnt = nasm_parser_input(s.lim, BSIZE)) != BSIZE){
	    s.eof = &s.lim[cnt]; *s.eof++ = '\n';
	}
	s.lim += cnt;
    }
    return cursor;
}

void
nasm_parser_cleanup(void)
{
    if (s.bot)
	xfree(s.bot);
}

/* starting size of string buffer */
#define STRBUF_ALLOC_SIZE	128

/* string buffer used when parsing strings/character constants */
static char *strbuf = (char *)NULL;

/* length of strbuf (including terminating NULL character) */
static size_t strbuf_size = 0;

/* last "base" label for local (.) labels */
char *nasm_parser_locallabel_base = (char *)NULL;
size_t nasm_parser_locallabel_base_len = 0;

static int linechg_numcount;

/*!re2c
  any = [\000-\377];
  digit = [0-9];
  iletter = [a-zA-Z];
  bindigit = [01];
  octdigit = [0-7];
  hexdigit = [0-9a-fA-F];
  ws = [ \t\r];
  quot = ["'];
  A = [aA];
  B = [bB];
  C = [cC];
  D = [dD];
  E = [eE];
  F = [fF];
  G = [gG];
  H = [hH];
  I = [iI];
  J = [jJ];
  K = [kK];
  L = [lL];
  M = [mM];
  N = [nN];
  O = [oO];
  P = [pP];
  Q = [qQ];
  R = [rR];
  S = [sS];
  T = [tT];
  U = [uU];
  V = [vV];
  W = [wW];
  X = [xX];
  Y = [yY];
  Z = [zZ];
*/

static enum {
    INITIAL,
    DIRECTIVE,
    DIRECTIVE2,
    LINECHG,
    LINECHG2
} state = INITIAL;

void
nasm_parser_set_directive_state(void)
{
    state = DIRECTIVE;
}

int
nasm_parser_lex(void)
{
    YYCTYPE *cursor = s.cur;
    YYCTYPE endch;
    size_t count, len;
    YYCTYPE savech;
    arch_check_id_retval check_id_ret;

    /* Catch EOF */
    if (s.eof && cursor == s.eof)
	return 0;

    /* Jump to proper "exclusive" states */
    switch (state) {
	case DIRECTIVE:
	    goto directive;
	case LINECHG:
	    goto linechg;
	case LINECHG2:
	    goto linechg2;
	default:
	    break;
    }

scan:
    SCANINIT();

    /*!re2c
	/* standard decimal integer */
	digit+ {
	    savech = s.tok[TOKLEN];
	    s.tok[TOKLEN] = '\0';
	    yylval.intn = intnum_new_dec(s.tok);
	    s.tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}
	/* 10010011b - binary number */

	bindigit+ "b" {
	    s.tok[TOKLEN-1] = '\0'; /* strip off 'b' */
	    yylval.intn = intnum_new_bin(s.tok);
	    RETURN(INTNUM);
	}

	/* 777q - octal number */
	octdigit+ "q" {
	    s.tok[TOKLEN-1] = '\0'; /* strip off 'q' */
	    yylval.intn = intnum_new_oct(s.tok);
	    RETURN(INTNUM);
	}

	/* 0AAh form of hexidecimal number */
	digit hexdigit* "h" {
	    s.tok[TOKLEN-1] = '\0'; /* strip off 'h' */
	    yylval.intn = intnum_new_hex(s.tok);
	    RETURN(INTNUM);
	}

	/* $0AA and 0xAA forms of hexidecimal number */
	(("$" digit) | "0x") hexdigit+ {
	    savech = s.tok[TOKLEN];
	    s.tok[TOKLEN] = '\0';
	    if (s.tok[1] == 'x')
		yylval.intn = intnum_new_hex(s.tok+2);	/* skip 0 and x */
	    else
		yylval.intn = intnum_new_hex(s.tok+1);	/* don't skip 0 */
	    s.tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}

	/* floating point value */
	digit+ "." digit* ("e" [-+]? digit+)? {
	    savech = s.tok[TOKLEN];
	    s.tok[TOKLEN] = '\0';
	    yylval.flt = floatnum_new(s.tok);
	    s.tok[TOKLEN] = savech;
	    RETURN(FLTNUM);
	}

	/* string/character constant values */
	quot {
	    endch = s.tok[0];
	    goto stringconst;
	}

	/* %line linenum+lineinc filename */
	"%line" {
	    state = LINECHG;
	    linechg_numcount = 0;
	    RETURN(LINE);
	}

	/* size specifiers */
	B Y T E		{ yylval.int_info = 1; RETURN(BYTE); }
	W O R D		{ yylval.int_info = 2; RETURN(WORD); }
	D W O R D	{ yylval.int_info = 4; RETURN(DWORD); }
	Q W O R D	{ yylval.int_info = 8; RETURN(QWORD); }
	T W O R D	{ yylval.int_info = 10; RETURN(TWORD); }
	D Q W O R D	{ yylval.int_info = 16; RETURN(DQWORD); }

	/* pseudo-instructions */
	D B		{ yylval.int_info = 1; RETURN(DECLARE_DATA); }
	D W		{ yylval.int_info = 2; RETURN(DECLARE_DATA); }
	D D		{ yylval.int_info = 4; RETURN(DECLARE_DATA); }
	D Q		{ yylval.int_info = 8; RETURN(DECLARE_DATA); }
	D T		{ yylval.int_info = 10; RETURN(DECLARE_DATA); }

	R E S B		{ yylval.int_info = 1; RETURN(RESERVE_SPACE); }
	R E S W		{ yylval.int_info = 2; RETURN(RESERVE_SPACE); }
	R E S D		{ yylval.int_info = 4; RETURN(RESERVE_SPACE); }
	R E S Q		{ yylval.int_info = 8; RETURN(RESERVE_SPACE); }
	R E S T		{ yylval.int_info = 10; RETURN(RESERVE_SPACE); }

	I N C B I N	{ RETURN(INCBIN); }

	E Q U		{ RETURN(EQU); }

	T I M E S	{ RETURN(TIMES); }

	S E G		{ RETURN(SEG); }
	W R T		{ RETURN(WRT); }

	N O S P L I T	{ RETURN(NOSPLIT); }

	/* operators */
	"<<"			{ RETURN(LEFT_OP); }
	">>"			{ RETURN(RIGHT_OP); }
	"//"			{ RETURN(SIGNDIV); }
	"%%"			{ RETURN(SIGNMOD); }
	"$$"			{ RETURN(START_SECTION_ID); }
	[-+|^*&/%~$():=,\[]	{ RETURN(s.tok[0]); }

	/* handle ] separately for directives */
	"]" {
	    if (state == DIRECTIVE2)
		state = INITIAL;
	    RETURN(s.tok[0]);
	}

	/* special non-local ..@label and labels like ..start */
	".." [a-zA-Z0-9_$#@~.?]+ {
	    yylval.str_val = xstrndup(s.tok, TOKLEN);
	    RETURN(SPECIAL_ID);
	}

	/* local label (.label) */
	"." [a-zA-Z0-9_$#@~?][a-zA-Z0-9_$#@~.?]* {
	    /* override local labels in directive state */
	    if (state == DIRECTIVE2) {
		yylval.str_val = xstrndup(s.tok, TOKLEN);
		RETURN(ID);
	    } else if (!nasm_parser_locallabel_base) {
		Warning(_("no non-local label before `%s'"), s.tok[0]);
		yylval.str_val = xstrndup(s.tok, TOKLEN);
	    } else {
		len = TOKLEN + nasm_parser_locallabel_base_len;
		yylval.str_val = xmalloc(len + 1);
		strcpy(yylval.str_val, nasm_parser_locallabel_base);
		strncat(yylval.str_val, s.tok, TOKLEN);
		yylval.str_val[len] = '\0';
	    }

	    RETURN(LOCAL_ID);
	}

	/* forced identifier */
	"$" [a-zA-Z_?][a-zA-Z0-9_$#@~.?]* {
	    yylval.str_val = xstrndup(s.tok, TOKLEN);
	    RETURN(ID);
	}

	/* identifier that may be a register, instruction, etc. */
	[a-zA-Z_?][a-zA-Z0-9_$#@~.?]* {
	    savech = s.tok[TOKLEN];
	    s.tok[TOKLEN] = '\0';
	    check_id_ret = cur_arch->parse.check_identifier(yylval.arch_data,
							    s.tok);
	    s.tok[TOKLEN] = savech;
	    switch (check_id_ret) {
		case ARCH_CHECK_ID_NONE:
		    /* Just an identifier, return as such. */
		    yylval.str_val = xstrndup(s.tok, TOKLEN);
		    RETURN(ID);
		case ARCH_CHECK_ID_INSN:
		    RETURN(INSN);
		case ARCH_CHECK_ID_PREFIX:
		    RETURN(PREFIX);
		case ARCH_CHECK_ID_REG:
		    RETURN(REG);
		case ARCH_CHECK_ID_SEGREG:
		    RETURN(SEGREG);
		case ARCH_CHECK_ID_TARGETMOD:
		    RETURN(TARGETMOD);
		default:
		    Warning(_("Arch feature not supported, treating as identifier"));
		    yylval.str_val = xstrndup(s.tok, TOKLEN);
		    RETURN(ID);
	    }
	}

	";" (any \ [\n])*	{ goto scan; }

	ws+			{ goto scan; }

	"\n"			{ state = INITIAL; RETURN(s.tok[0]); }

	any {
	    if (WARN_ENABLED(WARN_UNRECOGNIZED_CHAR))
		Warning(_("ignoring unrecognized character `%s'"),
			conv_unprint(s.tok[0]));
	    goto scan;
	}
    */

    /* %line linenum+lineinc filename */
linechg:
    SCANINIT();

    /*!re2c
	digit+ {
	    linechg_numcount++;
	    savech = s.tok[TOKLEN];
	    s.tok[TOKLEN] = '\0';
	    yylval.intn = intnum_new_dec(s.tok);
	    s.tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}

	"\n" {
	    state = INITIAL;
	    RETURN(s.tok[0]);
	}

	"+" {
	    RETURN(s.tok[0]);
	}

	ws+ {
	    if (linechg_numcount == 2)
	    state = LINECHG2;
	    goto linechg2;
	}

	any {
	    if (WARN_ENABLED(WARN_UNRECOGNIZED_CHAR))
		Warning(_("ignoring unrecognized character `%s'"),
			conv_unprint(s.tok[0]));
	    goto linechg;
	}
    */

linechg2:
    SCANINIT();

    /*!re2c
	"\n" {
	    state = INITIAL;
	    RETURN(s.tok[0]);
	}

	"\r" { }

	(any \ [\r\n])+	{
	    state = LINECHG;
	    yylval.str_val = xstrndup(s.tok, TOKLEN);
	    RETURN(FILENAME);
	}
    */

    /* directive: [name value] */
directive:
    SCANINIT();

    /*!re2c
	[\]\n] {
	    state = INITIAL;
	    RETURN(s.tok[0]);
	}

	iletter+ {
	    state = DIRECTIVE2;
	    yylval.str_val = xstrndup(s.tok, TOKLEN);
	    RETURN(DIRECTIVE_NAME);
	}

	any {
	    if (WARN_ENABLED(WARN_UNRECOGNIZED_CHAR))
		Warning(_("ignoring unrecognized character `%s'"),
			conv_unprint(s.tok[0]));
	    goto directive;
	}
    */

    /* string/character constant values */
stringconst:
    strbuf = xmalloc(STRBUF_ALLOC_SIZE);
    strbuf_size = STRBUF_ALLOC_SIZE;
    count = 0;

stringconst_scan:
    SCANINIT();

    /*!re2c
	"\n"	{
	    if (cursor == s.eof)
		Error(_("unexpected end of file in string"));
	    else
		Error(_("unterminated string"));
	    strbuf[count] = '\0';
	    yylval.str_val = strbuf;
	    RETURN(STRING);
	}

	any	{
	    if (s.tok[0] == endch) {
		strbuf[count] = '\0';
		yylval.str_val = strbuf;
		RETURN(STRING);
	    }

	    strbuf[count++] = s.tok[0];
	    if (count >= strbuf_size) {
		strbuf = xrealloc(strbuf, strbuf_size + STRBUF_ALLOC_SIZE);
		strbuf_size += STRBUF_ALLOC_SIZE;
	    }

	    goto stringconst_scan;
	}
    */
}
