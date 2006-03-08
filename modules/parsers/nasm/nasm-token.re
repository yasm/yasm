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
	    memmove(s->bot, s->tok, (size_t)(s->lim - s->tok));
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
	    char *saveline;
	    parser_nasm->save_last ^= 1;
	    saveline = parser_nasm->save_line[parser_nasm->save_last];
	    /* save next line into cur_line */
	    for (i=0; i<79 && &s->tok[i] < s->lim && s->tok[i] != '\n'; i++)
		saveline[i] = s->tok[i];
	    saveline[i] = '\0';
	}
    }
    return cursor;
}

static YYCTYPE *
save_line(yasm_parser_nasm *parser_nasm, YYCTYPE *cursor)
{
    Scanner *s = &parser_nasm->s;
    int i = 0;
    char *saveline;

    parser_nasm->save_last ^= 1;
    saveline = parser_nasm->save_line[parser_nasm->save_last];

    /* save next line into cur_line */
    if ((YYLIMIT - YYCURSOR) < 80)
	YYFILL(80);
    for (i=0; i<79 && &cursor[i] < s->lim && cursor[i] != '\n'; i++)
	saveline[i] = cursor[i];
    saveline[i] = '\0';
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
*/


int
nasm_parser_lex(YYSTYPE *lvalp, yasm_parser_nasm *parser_nasm)
{
    Scanner *s = &parser_nasm->s;
    YYCTYPE *cursor = s->cur;
    YYCTYPE endch;
    size_t count, len;
    YYCTYPE savech;

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

	bindigit+ 'b' {
	    s->tok[TOKLEN-1] = '\0'; /* strip off 'b' */
	    lvalp->intn = yasm_intnum_create_bin(s->tok, cur_line);
	    RETURN(INTNUM);
	}

	/* 777q or 777o - octal number */
	octdigit+ [qQoO] {
	    s->tok[TOKLEN-1] = '\0'; /* strip off 'q' or 'o' */
	    lvalp->intn = yasm_intnum_create_oct(s->tok, cur_line);
	    RETURN(INTNUM);
	}

	/* 0AAh form of hexidecimal number */
	digit hexdigit* 'h' {
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
	digit+ "." digit* ('e' [-+]? digit+)? {
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
	'byte'		{ lvalp->int_info = 1; RETURN(SIZE_OVERRIDE); }
	'hword'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)/2;
	    RETURN(SIZE_OVERRIDE);
	}
	'word'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch);
	    RETURN(SIZE_OVERRIDE);
	}
	'dword' | 'long'	{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*2;
	    RETURN(SIZE_OVERRIDE);
	}
	'qword'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*4;
	    RETURN(SIZE_OVERRIDE);
	}
	'tword'		{ lvalp->int_info = 10; RETURN(SIZE_OVERRIDE); }
	'dqword'	{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*8;
	    RETURN(SIZE_OVERRIDE);
	}

	/* pseudo-instructions */
	'db'		{ lvalp->int_info = 1; RETURN(DECLARE_DATA); }
	'dhw'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)/2;
	    RETURN(DECLARE_DATA);
	}
	'dw'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch);
	    RETURN(DECLARE_DATA);
	}
	'dd'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*2;
	    RETURN(DECLARE_DATA);
	}
	'dq'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*4;
	    RETURN(DECLARE_DATA);
	}
	'dt'		{ lvalp->int_info = 10; RETURN(DECLARE_DATA); }
	'ddq'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*8;
	    RETURN(DECLARE_DATA);
	}

	'resb'		{ lvalp->int_info = 1; RETURN(RESERVE_SPACE); }
	'reshw'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)/2;
	    RETURN(RESERVE_SPACE);
	}
	'resw'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch);
	    RETURN(RESERVE_SPACE);
	}
	'resd'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*2;
	    RETURN(RESERVE_SPACE);
	}
	'resq'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*4;
	    RETURN(RESERVE_SPACE);
	}
	'rest'		{ lvalp->int_info = 10; RETURN(RESERVE_SPACE); }
	'resdq'		{
	    lvalp->int_info = yasm_arch_wordsize(parser_nasm->arch)*8;
	    RETURN(RESERVE_SPACE);
	}

	'incbin'	{ RETURN(INCBIN); }

	'equ'		{ RETURN(EQU); }

	'times'		{ RETURN(TIMES); }

	'seg'		{ RETURN(SEG); }
	'wrt'		{ RETURN(WRT); }

	'nosplit'	{ RETURN(NOSPLIT); }

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
	    if (parser_nasm->state != INSTRUCTION)
		switch (yasm_arch_parse_check_insnprefix
			(parser_nasm->arch, lvalp->arch_data, s->tok, TOKLEN,
			 cur_line)) {
		    case YASM_ARCH_INSN:
			parser_nasm->state = INSTRUCTION;
			s->tok[TOKLEN] = savech;
			RETURN(INSN);
		    case YASM_ARCH_PREFIX:
			s->tok[TOKLEN] = savech;
			RETURN(PREFIX);
		    default:
			break;
		}
	    switch (yasm_arch_parse_check_regtmod
		    (parser_nasm->arch, lvalp->arch_data, s->tok, TOKLEN,
		     cur_line)) {
		case YASM_ARCH_REG:
		    s->tok[TOKLEN] = savech;
		    RETURN(REG);
		case YASM_ARCH_SEGREG:
		    s->tok[TOKLEN] = savech;
		    RETURN(SEGREG);
		case YASM_ARCH_TARGETMOD:
		    s->tok[TOKLEN] = savech;
		    RETURN(TARGETMOD);
		default:
		    s->tok[TOKLEN] = savech;
	    }
	    /* Just an identifier, return as such. */
	    lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
	    RETURN(ID);
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
	    lvalp->str.contents = strbuf;
	    lvalp->str.len = count;
	    if (parser_nasm->save_input && cursor != s->eof)
		cursor = save_line(parser_nasm, cursor);
	    RETURN(STRING);
	}

	any	{
	    if (s->tok[0] == endch) {
		strbuf[count] = '\0';
		lvalp->str.contents = strbuf;
		lvalp->str.len = count;
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
