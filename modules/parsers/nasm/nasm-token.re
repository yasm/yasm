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
#include <util.h>
RCSID("$Id$");

#define YASM_LIB_INTERNAL
#include <libyasm.h>

#include "modules/parsers/nasm/nasm-parser.h"
#include "modules/parsers/nasm/nasm-defs.h"


#define BSIZE	8192

#define YYCURSOR	cursor
#define YYLIMIT		(s->lim)
#define YYMARKER	(s->ptr)
#define YYFILL(n)	{cursor = fill(parser_nasm, cursor);}

#define RETURN(i)	{s->cur = cursor; return i;}

#define SCANINIT()	{ \
	s->tchar = cursor - s->pos; \
	s->tline = s->cline; \
	s->tok = cursor; \
    }

#define TOKLEN		(size_t)(cursor-s->tok)


static YYCTYPE *
fill(yasm_parser_nasm *parser_nasm, YYCTYPE *cursor)
{
    Scanner *s = &parser_nasm->s;
    int first = 0;
    if(!s->eof){
	size_t cnt = s->tok - s->bot;
	if(cnt){
	    memcpy(s->bot, s->tok, (size_t)(s->lim - s->tok));
	    s->tok = s->bot;
	    s->ptr -= cnt;
	    cursor -= cnt;
	    s->pos -= cnt;
	    s->lim -= cnt;
	}
	if (!s->bot)
	    first = 1;
	if((s->top - s->lim) < BSIZE){
	    char *buf = yasm_xmalloc((size_t)(s->lim - s->bot) + BSIZE);
	    memcpy(buf, s->tok, (size_t)(s->lim - s->tok));
	    s->tok = buf;
	    s->ptr = &buf[s->ptr - s->bot];
	    cursor = &buf[cursor - s->bot];
	    s->pos = &buf[s->pos - s->bot];
	    s->lim = &buf[s->lim - s->bot];
	    s->top = &s->lim[BSIZE];
	    if (s->bot)
		yasm_xfree(s->bot);
	    s->bot = buf;
	}
	if((cnt = yasm_preproc_input(parser_nasm->preproc, s->lim,
				     BSIZE)) == 0) {
	    s->eof = &s->lim[cnt]; *s->eof++ = '\n';
	}
	s->lim += cnt;
	if (first && parser_nasm->save_input) {
	    int i;
	    /* save next line into cur_line */
	    for (i=0; i<79 && &s->tok[i] < s->lim && s->tok[i] != '\n'; i++)
		parser_nasm->save_line[i] = s->tok[i];
	    parser_nasm->save_line[i] = '\0';
	}
    }
    return cursor;
}

static void
destroy_line(/*@only@*/ void *data)
{
    yasm_xfree(data);
}

static void
print_line(void *data, FILE *f, int indent_level)
{
    fprintf(f, "%*s\"%s\"\n", indent_level, "", (char *)data);
}

static const yasm_assoc_data_callback line_assoc_data = {
    destroy_line,
    print_line
};

static YYCTYPE *
save_line(yasm_parser_nasm *parser_nasm, YYCTYPE *cursor)
{
    Scanner *s = &parser_nasm->s;
    int i = 0;

    /* save previous line using assoc_data */
    yasm_linemap_add_data(parser_nasm->linemap, &line_assoc_data,
			  yasm__xstrdup(parser_nasm->save_line), 1);
    /* save next line into cur_line */
    if ((YYLIMIT - YYCURSOR) < 80)
	YYFILL(80);
    for (i=0; i<79 && &cursor[i] < s->lim && cursor[i] != '\n'; i++)
	parser_nasm->save_line[i] = cursor[i];
    parser_nasm->save_line[i] = '\0';
    return cursor;
}

void
nasm_parser_cleanup(yasm_parser_nasm *parser_nasm)
{
    if (parser_nasm->s.bot)
	yasm_xfree(parser_nasm->s.bot);
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


int
nasm_parser_lex(YYSTYPE *lvalp, yasm_parser_nasm *parser_nasm)
{
    Scanner *s = &parser_nasm->s;
    YYCTYPE *cursor = s->cur;
    YYCTYPE endch;
    size_t count, len;
    YYCTYPE savech;
    yasm_arch_check_id_retval check_id_ret;

    /* Catch EOF */
    if (s->eof && cursor == s->eof)
	return 0;

    /* Jump to proper "exclusive" states */
    switch (parser_nasm->state) {
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
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    lvalp->intn = yasm_intnum_create_dec(s->tok, cur_line);
	    s->tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}
	/* 10010011b - binary number */

	bindigit+ "b" {
	    s->tok[TOKLEN-1] = '\0'; /* strip off 'b' */
	    lvalp->intn = yasm_intnum_create_bin(s->tok, cur_line);
	    RETURN(INTNUM);
	}

	/* 777q - octal number */
	octdigit+ "q" {
	    s->tok[TOKLEN-1] = '\0'; /* strip off 'q' */
	    lvalp->intn = yasm_intnum_create_oct(s->tok, cur_line);
	    RETURN(INTNUM);
	}

	/* 0AAh form of hexidecimal number */
	digit hexdigit* "h" {
	    s->tok[TOKLEN-1] = '\0'; /* strip off 'h' */
	    lvalp->intn = yasm_intnum_create_hex(s->tok, cur_line);
	    RETURN(INTNUM);
	}

	/* $0AA and 0xAA forms of hexidecimal number */
	(("$" digit) | "0x") hexdigit+ {
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    if (s->tok[1] == 'x')
		/* skip 0 and x */
		lvalp->intn = yasm_intnum_create_hex(s->tok+2, cur_line);
	    else
		/* don't skip 0 */
		lvalp->intn = yasm_intnum_create_hex(s->tok+1, cur_line);
	    s->tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}

	/* floating point value */
	digit+ "." digit* (E [-+]? digit+)? {
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    lvalp->flt = yasm_floatnum_create(s->tok);
	    s->tok[TOKLEN] = savech;
	    RETURN(FLTNUM);
	}

	/* string/character constant values */
	quot {
	    endch = s->tok[0];
	    goto stringconst;
	}

	/* %line linenum+lineinc filename */
	"%line" {
	    parser_nasm->state = LINECHG;
	    linechg_numcount = 0;
	    RETURN(LINE);
	}

	/* size specifiers */
	B Y T E		{ lvalp->int_info = 1; RETURN(SIZE_OVERRIDE); }
	H W O R D	{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)/2;
	    RETURN(SIZE_OVERRIDE);
	}
	W O R D		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch);
	    RETURN(SIZE_OVERRIDE);
	}
	D W O R D	{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*2;
	    RETURN(SIZE_OVERRIDE);
	}
	Q W O R D	{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*4;
	    RETURN(SIZE_OVERRIDE);
	}
	T W O R D	{ lvalp->int_info = 10; RETURN(SIZE_OVERRIDE); }
	D Q W O R D	{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*8;
	    RETURN(SIZE_OVERRIDE);
	}

	/* pseudo-instructions */
	D B		{ lvalp->int_info = 1; RETURN(DECLARE_DATA); }
	D H W		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)/2;
	    RETURN(DECLARE_DATA);
	}
	D W		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch);
	    RETURN(DECLARE_DATA);
	}
	D D		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*2;
	    RETURN(DECLARE_DATA);
	}
	D Q		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*4;
	    RETURN(DECLARE_DATA);
	}
	D T		{ lvalp->int_info = 10; RETURN(DECLARE_DATA); }
	D D Q		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*8;
	    RETURN(DECLARE_DATA);
	}

	R E S B		{ lvalp->int_info = 1; RETURN(RESERVE_SPACE); }
	R E S H W	{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)/2;
	    RETURN(RESERVE_SPACE);
	}
	R E S W		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch);
	    RETURN(RESERVE_SPACE);
	}
	R E S D		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*2;
	    RETURN(RESERVE_SPACE);
	}
	R E S Q		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*4;
	    RETURN(RESERVE_SPACE);
	}
	R E S T		{ lvalp->int_info = 10; RETURN(RESERVE_SPACE); }
	R E S D Q	{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*8;
	    RETURN(RESERVE_SPACE);
	}

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
	[-+|^*&/%~$():=,\[]	{ RETURN(s->tok[0]); }

	/* handle ] separately for directives */
	"]" {
	    if (parser_nasm->state == DIRECTIVE2)
		parser_nasm->state = INITIAL;
	    RETURN(s->tok[0]);
	}

	/* special non-local ..@label and labels like ..start */
	".." [a-zA-Z0-9_$#@~.?]+ {
	    lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
	    RETURN(SPECIAL_ID);
	}

	/* local label (.label) */
	"." [a-zA-Z0-9_$#@~?][a-zA-Z0-9_$#@~.?]* {
	    /* override local labels in directive state */
	    if (parser_nasm->state == DIRECTIVE2) {
		lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
		RETURN(ID);
	    } else if (!parser_nasm->locallabel_base) {
		lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
		yasm__warning(YASM_WARN_GENERAL, cur_line,
			      N_("no non-local label before `%s'"),
			      lvalp->str_val);
	    } else {
		len = TOKLEN + parser_nasm->locallabel_base_len;
		lvalp->str_val = yasm_xmalloc(len + 1);
		strcpy(lvalp->str_val, parser_nasm->locallabel_base);
		strncat(lvalp->str_val, s->tok, TOKLEN);
		lvalp->str_val[len] = '\0';
	    }

	    RETURN(LOCAL_ID);
	}

	/* forced identifier */
	"$" [a-zA-Z_?][a-zA-Z0-9_$#@~.?]* {
	    lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
	    RETURN(ID);
	}

	/* identifier that may be a register, instruction, etc. */
	[a-zA-Z_?][a-zA-Z0-9_$#@~.?]* {
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    check_id_ret = yasm_arch_parse_check_id(parser_nasm->arch,
						    lvalp->arch_data, s->tok,
						    cur_line);
	    s->tok[TOKLEN] = savech;
	    switch (check_id_ret) {
		case YASM_ARCH_CHECK_ID_NONE:
		    /* Just an identifier, return as such. */
		    lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
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
		    yasm__warning(YASM_WARN_GENERAL, cur_line,
			N_("Arch feature not supported, treating as identifier"));
		    lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
		    RETURN(ID);
	    }
	}

	";" (any \ [\n])*	{ goto scan; }

	ws+			{ goto scan; }

	"\n"			{
	    if (parser_nasm->save_input && cursor != s->eof)
		cursor = save_line(parser_nasm, cursor);
	    parser_nasm->state = INITIAL;
	    RETURN(s->tok[0]);
	}

	any {
	    yasm__warning(YASM_WARN_UNREC_CHAR, cur_line,
			  N_("ignoring unrecognized character `%s'"),
			  yasm__conv_unprint(s->tok[0]));
	    goto scan;
	}
    */

    /* %line linenum+lineinc filename */
linechg:
    SCANINIT();

    /*!re2c
	digit+ {
	    linechg_numcount++;
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    lvalp->intn = yasm_intnum_create_dec(s->tok, cur_line);
	    s->tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}

	"\n" {
	    if (parser_nasm->save_input && cursor != s->eof)
		cursor = save_line(parser_nasm, cursor);
	    parser_nasm->state = INITIAL;
	    RETURN(s->tok[0]);
	}

	"+" {
	    RETURN(s->tok[0]);
	}

	ws+ {
	    if (linechg_numcount == 2) {
		parser_nasm->state = LINECHG2;
		goto linechg2;
	    }
	    goto linechg;
	}

	any {
	    yasm__warning(YASM_WARN_UNREC_CHAR, cur_line,
			  N_("ignoring unrecognized character `%s'"),
			  yasm__conv_unprint(s->tok[0]));
	    goto linechg;
	}
    */

linechg2:
    SCANINIT();

    /*!re2c
	"\n" {
	    if (parser_nasm->save_input && cursor != s->eof)
		cursor = save_line(parser_nasm, cursor);
	    parser_nasm->state = INITIAL;
	    RETURN(s->tok[0]);
	}

	"\r" { }

	(any \ [\r\n])+	{
	    parser_nasm->state = LINECHG;
	    lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
	    RETURN(FILENAME);
	}
    */

    /* directive: [name value] */
directive:
    SCANINIT();

    /*!re2c
	[\]\n] {
	    if (parser_nasm->save_input && cursor != s->eof)
		cursor = save_line(parser_nasm, cursor);
	    parser_nasm->state = INITIAL;
	    RETURN(s->tok[0]);
	}

	iletter+ {
	    parser_nasm->state = DIRECTIVE2;
	    lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
	    RETURN(DIRECTIVE_NAME);
	}

	any {
	    yasm__warning(YASM_WARN_UNREC_CHAR, cur_line,
			  N_("ignoring unrecognized character `%s'"),
			  yasm__conv_unprint(s->tok[0]));
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
	    if (cursor == s->eof)
		yasm__error(cur_line,
			    N_("unexpected end of file in string"));
	    else
		yasm__error(cur_line, N_("unterminated string"));
	    strbuf[count] = '\0';
	    lvalp->str_val = strbuf;
	    if (parser_nasm->save_input && cursor != s->eof)
		cursor = save_line(parser_nasm, cursor);
	    RETURN(STRING);
	}

	any	{
	    if (s->tok[0] == endch) {
		strbuf[count] = '\0';
		lvalp->str_val = strbuf;
		RETURN(STRING);
	    }

	    strbuf[count++] = s->tok[0];
	    if (count >= strbuf_size) {
		strbuf = yasm_xrealloc(strbuf, strbuf_size + STRBUF_ALLOC_SIZE);
		strbuf_size += STRBUF_ALLOC_SIZE;
	    }

	    goto stringconst_scan;
	}
    */
}
