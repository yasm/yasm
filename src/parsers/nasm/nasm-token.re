/*
 * NASM-compatible re2c lexer
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 *  Portions based on re2c's example code.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "util.h"
RCSID("$IdPath$");

#include "bitvect.h"

#include "linemgr.h"
#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

#include "bytecode.h"

#include "arch.h"

#include "src/parsers/nasm/nasm-parser.h"
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

typedef struct Scanner {
    YYCTYPE		*bot, *tok, *ptr, *cur, *pos, *lim, *top, *eof;
    unsigned int	tchar, tline, cline;
} Scanner;

#define MAX_SAVED_LINE_LEN  80
static char cur_line[MAX_SAVED_LINE_LEN];

static Scanner s = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 1 };

static YYCTYPE *
fill(YYCTYPE *cursor)
{
    int first = 0;
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
	if (!s.bot)
	    first = 1;
	if((s.top - s.lim) < BSIZE){
	    char *buf = yasm_xmalloc((s.lim - s.bot) + BSIZE);
	    memcpy(buf, s.tok, s.lim - s.tok);
	    s.tok = buf;
	    s.ptr = &buf[s.ptr - s.bot];
	    cursor = &buf[cursor - s.bot];
	    s.pos = &buf[s.pos - s.bot];
	    s.lim = &buf[s.lim - s.bot];
	    s.top = &s.lim[BSIZE];
	    if (s.bot)
		yasm_xfree(s.bot);
	    s.bot = buf;
	}
	if((cnt = nasm_parser_input(s.lim, BSIZE)) == 0){
	    s.eof = &s.lim[cnt]; *s.eof++ = '\n';
	}
	s.lim += cnt;
	if (first && nasm_parser_save_input) {
	    int i;
	    /* save next line into cur_line */
	    for (i=0; i<79 && &s.tok[i] < s.lim && s.tok[i] != '\n'; i++)
		cur_line[i] = s.tok[i];
	    cur_line[i] = '\0';
	}
    }
    return cursor;
}

static void
delete_line(/*@only@*/ void *data)
{
    yasm_xfree(data);
}

static YYCTYPE *
save_line(YYCTYPE *cursor)
{
    int i = 0;

    /* save previous line using assoc_data */
    nasm_parser_linemgr->add_assoc_data(YASM_LINEMGR_STD_TYPE_SOURCE,
					yasm__xstrdup(cur_line), delete_line);
    /* save next line into cur_line */
    if ((YYLIMIT - YYCURSOR) < 80)
	YYFILL(80);
    for (i=0; i<79 && &cursor[i] < s.lim && cursor[i] != '\n'; i++)
	cur_line[i] = cursor[i];
    cur_line[i] = '\0';
    return cursor;
}

void
nasm_parser_cleanup(void)
{
    if (s.bot)
	yasm_xfree(s.bot);
}

/* starting size of string buffer */
#define STRBUF_ALLOC_SIZE	128

/* string buffer used when parsing strings/character constants */
static char *strbuf = (char *)NULL;

/* length of strbuf (including terminating NULL character) */
static size_t strbuf_size = 0;

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
    yasm_arch_check_id_retval check_id_ret;

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
	    yylval.intn = yasm_intnum_new_dec(s.tok, cur_lindex);
	    s.tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}
	/* 10010011b - binary number */

	bindigit+ "b" {
	    s.tok[TOKLEN-1] = '\0'; /* strip off 'b' */
	    yylval.intn = yasm_intnum_new_bin(s.tok, cur_lindex);
	    RETURN(INTNUM);
	}

	/* 777q - octal number */
	octdigit+ "q" {
	    s.tok[TOKLEN-1] = '\0'; /* strip off 'q' */
	    yylval.intn = yasm_intnum_new_oct(s.tok, cur_lindex);
	    RETURN(INTNUM);
	}

	/* 0AAh form of hexidecimal number */
	digit hexdigit* "h" {
	    s.tok[TOKLEN-1] = '\0'; /* strip off 'h' */
	    yylval.intn = yasm_intnum_new_hex(s.tok, cur_lindex);
	    RETURN(INTNUM);
	}

	/* $0AA and 0xAA forms of hexidecimal number */
	(("$" digit) | "0x") hexdigit+ {
	    savech = s.tok[TOKLEN];
	    s.tok[TOKLEN] = '\0';
	    if (s.tok[1] == 'x')
		/* skip 0 and x */
		yylval.intn = yasm_intnum_new_hex(s.tok+2, cur_lindex);
	    else
		/* don't skip 0 */
		yylval.intn = yasm_intnum_new_hex(s.tok+1, cur_lindex);
	    s.tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}

	/* floating point value */
	digit+ "." digit* (E [-+]? digit+)? {
	    savech = s.tok[TOKLEN];
	    s.tok[TOKLEN] = '\0';
	    yylval.flt = yasm_floatnum_new(s.tok);
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
	    yylval.str_val = yasm__xstrndup(s.tok, TOKLEN);
	    RETURN(SPECIAL_ID);
	}

	/* local label (.label) */
	"." [a-zA-Z0-9_$#@~?][a-zA-Z0-9_$#@~.?]* {
	    /* override local labels in directive state */
	    if (state == DIRECTIVE2) {
		yylval.str_val = yasm__xstrndup(s.tok, TOKLEN);
		RETURN(ID);
	    } else if (!nasm_parser_locallabel_base) {
		yasm__warning(YASM_WARN_GENERAL, cur_lindex,
			      N_("no non-local label before `%s'"), s.tok[0]);
		yylval.str_val = yasm__xstrndup(s.tok, TOKLEN);
	    } else {
		len = TOKLEN + nasm_parser_locallabel_base_len;
		yylval.str_val = yasm_xmalloc(len + 1);
		strcpy(yylval.str_val, nasm_parser_locallabel_base);
		strncat(yylval.str_val, s.tok, TOKLEN);
		yylval.str_val[len] = '\0';
	    }

	    RETURN(LOCAL_ID);
	}

	/* forced identifier */
	"$" [a-zA-Z_?][a-zA-Z0-9_$#@~.?]* {
	    yylval.str_val = yasm__xstrndup(s.tok, TOKLEN);
	    RETURN(ID);
	}

	/* identifier that may be a register, instruction, etc. */
	[a-zA-Z_?][a-zA-Z0-9_$#@~.?]* {
	    savech = s.tok[TOKLEN];
	    s.tok[TOKLEN] = '\0';
	    check_id_ret = nasm_parser_arch->parse.check_identifier(
		yylval.arch_data, s.tok, cur_lindex);
	    s.tok[TOKLEN] = savech;
	    switch (check_id_ret) {
		case YASM_ARCH_CHECK_ID_NONE:
		    /* Just an identifier, return as such. */
		    yylval.str_val = yasm__xstrndup(s.tok, TOKLEN);
		    RETURN(ID);
		case YASM_ARCH_CHECK_ID_INSN:
		    RETURN(INSN);
		case YASM_ARCH_CHECK_ID_PREFIX:
		    RETURN(PREFIX);
		case YASM_ARCH_CHECK_ID_REG:
		    RETURN(REG);
		case YASM_ARCH_CHECK_ID_SEGREG:
		    RETURN(SEGREG);
		case YASM_ARCH_CHECK_ID_TARGETMOD:
		    RETURN(TARGETMOD);
		default:
		    yasm__warning(YASM_WARN_GENERAL, cur_lindex,
			N_("Arch feature not supported, treating as identifier"));
		    yylval.str_val = yasm__xstrndup(s.tok, TOKLEN);
		    RETURN(ID);
	    }
	}

	";" (any \ [\n])*	{ goto scan; }

	ws+			{ goto scan; }

	"\n"			{
	    if (nasm_parser_save_input && cursor != s.eof)
		cursor = save_line(cursor);
	    state = INITIAL;
	    RETURN(s.tok[0]);
	}

	any {
	    yasm__warning(YASM_WARN_UNREC_CHAR, cur_lindex,
			  N_("ignoring unrecognized character `%s'"),
			  yasm__conv_unprint(s.tok[0]));
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
	    yylval.intn = yasm_intnum_new_dec(s.tok, cur_lindex);
	    s.tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}

	"\n" {
	    if (nasm_parser_save_input && cursor != s.eof)
		cursor = save_line(cursor);
	    state = INITIAL;
	    RETURN(s.tok[0]);
	}

	"+" {
	    RETURN(s.tok[0]);
	}

	ws+ {
	    if (linechg_numcount == 2) {
		state = LINECHG2;
		goto linechg2;
	    }
	    goto linechg;
	}

	any {
	    yasm__warning(YASM_WARN_UNREC_CHAR, cur_lindex,
			  N_("ignoring unrecognized character `%s'"),
			  yasm__conv_unprint(s.tok[0]));
	    goto linechg;
	}
    */

linechg2:
    SCANINIT();

    /*!re2c
	"\n" {
	    if (nasm_parser_save_input && cursor != s.eof)
		cursor = save_line(cursor);
	    state = INITIAL;
	    RETURN(s.tok[0]);
	}

	"\r" { }

	(any \ [\r\n])+	{
	    state = LINECHG;
	    yylval.str_val = yasm__xstrndup(s.tok, TOKLEN);
	    RETURN(FILENAME);
	}
    */

    /* directive: [name value] */
directive:
    SCANINIT();

    /*!re2c
	[\]\n] {
	    if (nasm_parser_save_input && cursor != s.eof)
		cursor = save_line(cursor);
	    state = INITIAL;
	    RETURN(s.tok[0]);
	}

	iletter+ {
	    state = DIRECTIVE2;
	    yylval.str_val = yasm__xstrndup(s.tok, TOKLEN);
	    RETURN(DIRECTIVE_NAME);
	}

	any {
	    yasm__warning(YASM_WARN_UNREC_CHAR, cur_lindex,
			  N_("ignoring unrecognized character `%s'"),
			  yasm__conv_unprint(s.tok[0]));
	    goto directive;
	}
    */

    /* string/character constant values */
stringconst:
    strbuf = yasm_xmalloc(STRBUF_ALLOC_SIZE);
    strbuf_size = STRBUF_ALLOC_SIZE;
    count = 0;

stringconst_scan:
    SCANINIT();

    /*!re2c
	"\n"	{
	    if (cursor == s.eof)
		yasm__error(cur_lindex,
			    N_("unexpected end of file in string"));
	    else
		yasm__error(cur_lindex, N_("unterminated string"));
	    strbuf[count] = '\0';
	    yylval.str_val = strbuf;
	    if (nasm_parser_save_input && cursor != s.eof)
		cursor = save_line(cursor);
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
		strbuf = yasm_xrealloc(strbuf, strbuf_size + STRBUF_ALLOC_SIZE);
		strbuf_size += STRBUF_ALLOC_SIZE;
	    }

	    goto stringconst_scan;
	}
    */
}
