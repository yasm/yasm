/*
 * GAS-compatible re2c lexer
 *
 *  Copyright (C) 2005  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "modules/parsers/gas/gas-parser.h"
#include "modules/parsers/gas/gas-defs.h"


#define BSIZE	8192

#define YYCURSOR	cursor
#define YYLIMIT		(s->lim)
#define YYMARKER	(s->ptr)
#define YYFILL(n)	{cursor = fill(parser_gas, cursor);}

#define RETURN(i)	{s->cur = cursor; return i;}

#define SCANINIT()	{ \
	s->tchar = cursor - s->pos; \
	s->tline = s->cline; \
	s->tok = cursor; \
    }

#define TOKLEN		(size_t)(cursor-s->tok)


static YYCTYPE *
fill(yasm_parser_gas *parser_gas, YYCTYPE *cursor)
{
    Scanner *s = &parser_gas->s;
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
	if((cnt = yasm_preproc_input(parser_gas->preproc, s->lim,
				     BSIZE)) == 0) {
	    s->eof = &s->lim[cnt]; *s->eof++ = '\n';
	}
	s->lim += cnt;
	if (first && parser_gas->save_input) {
	    int i;
	    char *saveline;
	    parser_gas->save_last ^= 1;
	    saveline = parser_gas->save_line[parser_gas->save_last];
	    /* save next line into cur_line */
	    for (i=0; i<79 && &s->tok[i] < s->lim && s->tok[i] != '\n'; i++)
		saveline[i] = s->tok[i];
	    saveline[i] = '\0';
	}
    }
    return cursor;
}

static YYCTYPE *
save_line(yasm_parser_gas *parser_gas, YYCTYPE *cursor)
{
    Scanner *s = &parser_gas->s;
    int i = 0;
    char *saveline;

    parser_gas->save_last ^= 1;
    saveline = parser_gas->save_line[parser_gas->save_last];

    /* save next line into cur_line */
    if ((YYLIMIT - YYCURSOR) < 80)
	YYFILL(80);
    for (i=0; i<79 && &cursor[i] < s->lim && cursor[i] != '\n'; i++)
	saveline[i] = cursor[i];
    saveline[i] = '\0';
    return cursor;
}

void
gas_parser_cleanup(yasm_parser_gas *parser_gas)
{
    if (parser_gas->s.bot)
	yasm_xfree(parser_gas->s.bot);
}

/* starting size of string buffer */
#define STRBUF_ALLOC_SIZE	128

/* string buffer used when parsing strings/character constants */
static char *strbuf = (char *)NULL;

/* length of strbuf (including terminating NULL character) */
static size_t strbuf_size = 0;

static void
strbuf_append(size_t count, YYCTYPE *cursor, Scanner *s, unsigned long line,
	      int ch)
{
    if (cursor == s->eof)
	yasm__error(line, N_("unexpected end of file in string"));

    if (count >= strbuf_size) {
	strbuf = yasm_xrealloc(strbuf, strbuf_size + STRBUF_ALLOC_SIZE);
	strbuf_size += STRBUF_ALLOC_SIZE;
    }
    strbuf[count] = ch;
}

/*!re2c
  any = [\000-\377];
  digit = [0-9];
  iletter = [a-zA-Z];
  bindigit = [01];
  octdigit = [0-7];
  hexdigit = [0-9a-fA-F];
  ws = [ \t\r];
  dquot = ["];
*/


int
gas_parser_lex(YYSTYPE *lvalp, yasm_parser_gas *parser_gas)
{
    Scanner *s = &parser_gas->s;
    YYCTYPE *cursor = s->cur;
    size_t count;
    YYCTYPE savech;

    /* Catch EOF */
    if (s->eof && cursor == s->eof)
	return 0;

    /* Jump to proper "exclusive" states */
    switch (parser_gas->state) {
	case SECTION_DIRECTIVE:
	    goto section_directive;
	default:
	    break;
    }

scan:
    SCANINIT();

    /*!re2c
	/* standard decimal integer */
	([1-9] digit*) | "0" {
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    lvalp->intn = yasm_intnum_create_dec(s->tok, cur_line);
	    s->tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}

	/* 0b10010011 - binary number */
	'0b' bindigit+ {
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    lvalp->intn = yasm_intnum_create_bin(s->tok+2, cur_line);
	    s->tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}

	/* 0777 - octal number */
	"0" octdigit+ {
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    lvalp->intn = yasm_intnum_create_oct(s->tok, cur_line);
	    s->tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}

	/* 0xAA - hexidecimal number */
	'0x' hexdigit+ {
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    /* skip 0 and x */
	    lvalp->intn = yasm_intnum_create_hex(s->tok+2, cur_line);
	    s->tok[TOKLEN] = savech;
	    RETURN(INTNUM);
	}

	/* floating point value */
	"0" [DdEeFfTt] [-+]? (digit+)? ("." digit*)? ('e' [-+]? digit+)? {
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    lvalp->flt = yasm_floatnum_create(s->tok+2);
	    s->tok[TOKLEN] = savech;
	    RETURN(FLTNUM);
	}

	/* character constant values */
	['] {
	    goto charconst;
	}

	/* string constant values */
	dquot {
	    goto stringconst;
	}

	/* operators */
	"<<"			{ RETURN(LEFT_OP); }
	">>"			{ RETURN(RIGHT_OP); }
	"<"			{ RETURN(LEFT_OP); }
	">"			{ RETURN(RIGHT_OP); }
	[-+|^!*&/~$():;@=,]	{ RETURN(s->tok[0]); }

	/* arch-independent directives */
	'.2byte'	{ RETURN(DIR_2BYTE); }
	'.4byte'	{ RETURN(DIR_4BYTE); }
	'.8byte'	{ RETURN(DIR_QUAD); }
	'.align'	{ RETURN(DIR_ALIGN); }
	'.ascii'	{ RETURN(DIR_ASCII); }
	'.asciz'	{ RETURN(DIR_ASCIZ); }
	'.balign'	{ RETURN(DIR_BALIGN); }
	'.bss'		{ RETURN(DIR_BSS); }
	'.byte'		{ RETURN(DIR_BYTE); }
	'.comm'		{ RETURN(DIR_COMM); }
	'.data'		{ RETURN(DIR_DATA); }
	'.double'	{ RETURN(DIR_DOUBLE); }
	'.endr'		{ RETURN(DIR_ENDR); }
	'.equ'		{ RETURN(DIR_EQU); }
	'.extern'	{ RETURN(DIR_EXTERN); }
	'.file'		{ RETURN(DIR_FILE); }
	'.float'	{ RETURN(DIR_FLOAT); }
	'.global'	{ RETURN(DIR_GLOBAL); }
	'.globl'	{ RETURN(DIR_GLOBAL); }
	'.hword'	{ RETURN(DIR_SHORT); }
	'.ident'	{ RETURN(DIR_IDENT); }
	'.int'		{ RETURN(DIR_INT); }
	'.lcomm'	{ RETURN(DIR_LCOMM); }
	'.loc'		{ RETURN(DIR_LOC); }
	'.long'		{ RETURN(DIR_INT); }
	'.octa'		{ RETURN(DIR_OCTA); }
	'.org'		{ RETURN(DIR_ORG); }
	'.p2align'	{ RETURN(DIR_P2ALIGN); }
	'.rept'		{ RETURN(DIR_REPT); }
	'.section'	{
	    parser_gas->state = SECTION_DIRECTIVE;
	    RETURN(DIR_SECTION);
	}
	'.set'		{ RETURN(DIR_EQU); }
	'.short'	{ RETURN(DIR_SHORT); }
	'.single'	{ RETURN(DIR_FLOAT); }
	'.size'		{ RETURN(DIR_SIZE); }
	'.skip'		{ RETURN(DIR_SKIP); }
	'.sleb128'	{ RETURN(DIR_SLEB128); }
	'.space'	{ RETURN(DIR_SKIP); }
	'.string'	{ RETURN(DIR_ASCIZ); }
	'.text'		{ RETURN(DIR_TEXT); }
	'.tfloat'	{ RETURN(DIR_TFLOAT); }
	'.type'		{ RETURN(DIR_TYPE); }
	'.quad'		{ RETURN(DIR_QUAD); }
	'.uleb128'	{ RETURN(DIR_ULEB128); }
	'.value'	{ RETURN(DIR_VALUE); }
	'.weak'		{ RETURN(DIR_WEAK); }
	'.word'		{ RETURN(DIR_WORD); }
	'.zero'		{ RETURN(DIR_ZERO); }

	/* label or maybe directive */
	[.][a-zA-Z0-9_$.]* {
	    lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
	    RETURN(DIR_ID);
	}

	/* label */
	[_][a-zA-Z0-9_$.]* {
	    lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
	    RETURN(ID);
	}

	/* register or segment register */
	[%][a-zA-Z0-9]+ {
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    if (yasm_arch_parse_check_reg(parser_gas->arch, lvalp->arch_data,
					  s->tok+1, cur_line)) {
		s->tok[TOKLEN] = savech;
		RETURN(REG);
	    }
	    if (yasm_arch_parse_check_reggroup(parser_gas->arch,
					       lvalp->arch_data, s->tok+1,
					       cur_line)) {
		s->tok[TOKLEN] = savech;
		RETURN(REGGROUP);
	    }
	    if (yasm_arch_parse_check_segreg(parser_gas->arch,
					     lvalp->arch_data, s->tok+1,
					     cur_line)) {
		s->tok[TOKLEN] = savech;
		RETURN(SEGREG);
	    }
	    yasm__error(cur_line, N_("Unrecognized register name `%s'"),
			s->tok);
	    s->tok[TOKLEN] = savech;
	    lvalp->arch_data[0] = 0;
	    lvalp->arch_data[1] = 0;
	    lvalp->arch_data[2] = 0;
	    lvalp->arch_data[3] = 0;
	    RETURN(REG);
	}

	/* identifier that may be an instruction, etc. */
	[a-zA-Z][a-zA-Z0-9_$.]* {
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    if (yasm_arch_parse_check_insn(parser_gas->arch, lvalp->arch_data,
					   s->tok, cur_line)) {
		s->tok[TOKLEN] = savech;
		RETURN(INSN);
	    }
	    if (yasm_arch_parse_check_prefix(parser_gas->arch,
					     lvalp->arch_data, s->tok,
					     cur_line)) {
		s->tok[TOKLEN] = savech;
		RETURN(PREFIX);
	    }
	    s->tok[TOKLEN] = savech;
	    /* Just an identifier, return as such. */
	    lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
	    RETURN(ID);
	}

	"#" (any \ [\n])*	{ goto scan; }

	ws+			{ goto scan; }

	"\n"			{
	    if (parser_gas->save_input && cursor != s->eof)
		cursor = save_line(parser_gas, cursor);
	    parser_gas->state = INITIAL;
	    RETURN(s->tok[0]);
	}

	any {
	    yasm__warning(YASM_WARN_UNREC_CHAR, cur_line,
			  N_("ignoring unrecognized character `%s'"),
			  yasm__conv_unprint(s->tok[0]));
	    goto scan;
	}
    */

    /* .section directive (the section name portion thereof) */
section_directive:
    SCANINIT();

    /*!re2c
	[a-zA-Z0-9_$.-]+ {
	    lvalp->str_val = yasm__xstrndup(s->tok, TOKLEN);
	    parser_gas->state = INITIAL;
	    RETURN(ID);
	}

	dquot			{ goto stringconst; }

	ws+			{ goto section_directive; }

	","			{
	    parser_gas->state = INITIAL;
	    RETURN(s->tok[0]);
	}

	"\n"			{
	    if (parser_gas->save_input && cursor != s->eof)
		cursor = save_line(parser_gas, cursor);
	    parser_gas->state = INITIAL;
	    RETURN(s->tok[0]);
	}

	any {
	    yasm__warning(YASM_WARN_UNREC_CHAR, cur_line,
			  N_("ignoring unrecognized character `%s'"),
			  yasm__conv_unprint(s->tok[0]));
	    goto section_directive;
	}
    */

    /* character constant values */
charconst:
    /*TODO*/

    /* string constant values */
stringconst:
    strbuf = yasm_xmalloc(STRBUF_ALLOC_SIZE);
    strbuf_size = STRBUF_ALLOC_SIZE;
    count = 0;

stringconst_scan:
    SCANINIT();

    /*!re2c
	/* Escaped constants */
	"\\b"	{
	    strbuf_append(count++, cursor, s, cur_line, '\b');
	    goto stringconst_scan;
	}
	"\\f"	{
	    strbuf_append(count++, cursor, s, cur_line, '\f');
	    goto stringconst_scan;
	}
	"\\n"	{
	    strbuf_append(count++, cursor, s, cur_line, '\n');
	    goto stringconst_scan;
	}
	"\\r"	{
	    strbuf_append(count++, cursor, s, cur_line, '\r');
	    goto stringconst_scan;
	}
	"\\t"	{
	    strbuf_append(count++, cursor, s, cur_line, '\t');
	    goto stringconst_scan;
	}
	"\\" digit digit digit	{
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    lvalp->intn = yasm_intnum_create_oct(s->tok+1, cur_line);
	    s->tok[TOKLEN] = savech;

	    strbuf_append(count++, cursor, s, cur_line,
			  yasm_intnum_get_int(lvalp->intn));
	    yasm_intnum_destroy(lvalp->intn);
	    goto stringconst_scan;
	}
	'\\x' hexdigit+    {
	    savech = s->tok[TOKLEN];
	    s->tok[TOKLEN] = '\0';
	    lvalp->intn = yasm_intnum_create_hex(s->tok+2, cur_line);
	    s->tok[TOKLEN] = savech;

	    strbuf_append(count++, cursor, s, cur_line,
			  yasm_intnum_get_int(lvalp->intn));
	    yasm_intnum_destroy(lvalp->intn);
	    goto stringconst_scan;
	}
	"\\\\"	    {
	    strbuf_append(count++, cursor, s, cur_line, '\\');
	    goto stringconst_scan;
	}
	"\\\""	    {
	    strbuf_append(count++, cursor, s, cur_line, '"');
	    goto stringconst_scan;
	}
	/*"\\" any    {
	    strbuf_append(count++, cursor, s, cur_line, s->tok[0]);
	    goto stringconst_scan;
	}*/

	dquot	{
	    strbuf_append(count, cursor, s, cur_line, '\0');
	    lvalp->str_val = strbuf;
	    RETURN(STRING);
	}

	any	{
	    strbuf_append(count++, cursor, s, cur_line, s->tok[0]);
	    goto stringconst_scan;
	}
    */
}
