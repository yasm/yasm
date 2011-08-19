/*
 * YAPP lexer
 *
 * Copyright (C) 2001  Michael Urman
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
%{
#include <util.h>

#define YASM_LIB_INTERNAL
#include <libyasm.h>

#include <errno.h>

#include "modules/preprocs/yapp/yapp-preproc.h"
#include "modules/preprocs/yapp/yapp-token.h"


#define yylval	yapp_preproc_lval

#define malloc yasm_xmalloc
#define realloc yasm_xrealloc

/* starting size of string buffer */
#define STRBUF_ALLOC_SIZE	128

/* string buffer used when parsing strings/character constants */
static char *strbuf = (char *)NULL;

/* length of strbuf (including terminating NULL character) */
static size_t strbuf_size = 0;

/* include file mumbo jumbo */
static SLIST_HEAD(include_head, include_s) includes_head;
struct include_s {
    SLIST_ENTRY(include_s) next;
    YY_BUFFER_STATE include_state;
    char *filename;
    int line_number;
};
typedef struct include_s include;

char *yapp_preproc_current_file;
int yapp_preproc_line_number;

%}
%option noyywrap
%option nounput
%option case-insensitive
%option prefix="yapp_preproc_"
%option outfile="lex.yy.c"

%x D
%x incl
%x line
%x inhibit

DIGIT    [0-9]
BINDIGIT [01]
OCTDIGIT [0-7]
HEXDIGIT [0-9a-f]
WS       [ \t]
DIR	 %[ \t]*

%%

    /* standard decimal integer */
{DIGIT}+ {
    yylval.int_str_val.val = strtoul(yytext, (char **)NULL, 10);
    yylval.int_str_val.str = yytext;
    return INTNUM;
}

    /* 10010011b - binary number */
{BINDIGIT}+b {
    yylval.int_str_val.val = strtoul(yytext, (char **)NULL, 2);
    yylval.int_str_val.str = yytext;
    return INTNUM;
}

    /* 777q - octal number */
{OCTDIGIT}+q {
    yylval.int_str_val.val = strtoul(yytext, (char **)NULL, 8);
    yylval.int_str_val.str = yytext;
    return INTNUM;
}

    /* 0AAh form of hexidecimal number */
0{HEXDIGIT}+h {
    yylval.int_str_val.val = strtoul(yytext+1, (char **)NULL, 16);
    yylval.int_str_val.str = yytext;
    return INTNUM;
}

    /* $0AA and 0xAA forms of hexidecimal number */
(\$0|0x){HEXDIGIT}+ {
    yylval.int_str_val.val = strtoul(yytext+2, (char **)NULL, 16);
    yylval.int_str_val.str = yytext;
    return INTNUM;
}

    /* floating point value */
{DIGIT}+\.{DIGIT}*(e[-+]?{DIGIT}+)? {
    yylval.double_str_val.val = strtod(yytext, (char **)NULL);
    yylval.double_str_val.str = yytext;
    return FLTNUM;
}

    /* string/character constant values */
["']	{
    int inch;
    size_t count;
    char endch = yytext[0];

    strbuf = yasm_xmalloc(STRBUF_ALLOC_SIZE);

    strbuf_size = STRBUF_ALLOC_SIZE;
    inch = input();
    count = 0;
    while(inch != EOF && inch != endch && inch != '\n') {
	strbuf[count++] = inch;
	if(count >= strbuf_size) {
	    strbuf = yasm_xrealloc(strbuf, strbuf_size + STRBUF_ALLOC_SIZE);
	    strbuf_size += STRBUF_ALLOC_SIZE;
	}
	inch = input();
    }

    if(inch == '\n')
	yasm_error_set(YASM_ERROR_VALUE, N_("unterminated string"));
    else if(inch == EOF)
	yasm_error_set(YASM_ERROR_VALUE, N_("unexpected end of file in string"));

    strbuf[count] = '\0';

    yylval.str_val = strbuf;
    return STRING;
}

    /* identifiers */
\.\.[a-z0-9_$#@~.?]+ |
\.[a-z0-9_$#@~?][a-z0-9_$#@~.?]* | 
[a-z_?][a-z0-9_$#@~.?]* {
    yylval.str_val = yasm__xstrdup(yytext);
    return IDENT;
}

    /* includes - based on flex manual handling of include files */
<inhibit>{DIR}include[^\n]* ;
{DIR}include  BEGIN(incl);
    /* note the " handling here is a hack that doesn't accept useful
     * things (like spaces, or quotes).  fix it later */
<incl>[ \t"]*	/* eat whitespace */
<incl>[^ \t\n"]* { /* have the filename */
    include *inc;
    FILE *incfile;
    inc = yasm_xmalloc(sizeof(include));
    inc->include_state = YY_CURRENT_BUFFER;

    /* FIXME: handle includes that aren't relative */
    incfile = fopen (yytext, "r");
    if(!incfile) {
	yasm_error_set(YASM_ERROR_VALUE, N_("include file `%s': %s"),
		    yytext, strerror(errno));
	yasm_xfree(inc);
    }
    else {
	yyin = incfile;
	inc->filename = yapp_preproc_current_file;
	inc->line_number = yapp_preproc_line_number;
	SLIST_INSERT_HEAD(&includes_head, inc, next);

	yapp_preproc_line_number = 1;
	yapp_preproc_current_file = yasm__xstrdup(yytext);
	BEGIN(INITIAL);
	yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
    }
    return INCLUDE;
}

    /* end includes - note that it's not in <incl> at the time */
<<EOF>> {
    if(SLIST_EMPTY(&includes_head)) {
	yyterminate();
    }
    else {
	include *inc;
	inc = SLIST_FIRST(&includes_head);
	yy_delete_buffer (YY_CURRENT_BUFFER);
	yy_switch_to_buffer (inc->include_state);
	yasm_xfree(yapp_preproc_current_file);
	yapp_preproc_current_file = inc->filename;
	yapp_preproc_line_number = inc->line_number + 1;
	SLIST_REMOVE_HEAD(&includes_head, next);
	yasm_xfree(inc);

	BEGIN(incl);
	return INCLUDE;
    }
}

<incl>["]{WS}*\n    BEGIN(INITIAL);


    /* directive: % directive [args] */
<inhibit>{DIR}clear[^\n]    ;
{DIR}clear    return CLEAR;

<inhibit>{DIR}line[^\n]    ;
{DIR}line     BEGIN(line);
<line>{DIGIT}+	    yapp_preproc_line_number = strtoul(yytext, (char **)NULL, 10);
<line>{DIGIT}+{WS}*\n	{
    yapp_preproc_line_number = strtoul(yytext, (char **)NULL, 10);
    BEGIN(INITIAL);
    return LINE;
}
<line>{WS}+["]	    ;	/* eat space before file */
<line>[^ \t\n"]*    { /* have the filename */
    yasm_xfree(yapp_preproc_current_file);
    yapp_preproc_current_file = yasm__xstrdup(yytext);
}
<line>["]{WS}*\n    {
    BEGIN(INITIAL);
    return LINE;
}

{DIR}define   return DEFINE;
{DIR}undef    return UNDEF;
{DIR}assign   return ASSIGN;
{DIR}macro    return MACRO;
{DIR}endmacro return ENDMACRO;
{DIR}rotate   return ROTATE;
<inhibit>{DIR}define[^\n]*	;
<inhibit>{DIR}undef[^\n]*	;
<inhibit>{DIR}assign[^\n]*	;
<inhibit>{DIR}macro[^\n]*	;
<inhibit>{DIR}endmacro[^\n]*	;
<inhibit>{DIR}rotate[^\n]*	;

    /* preprocessor loops */
{DIR}rep      return REP;
{DIR}exitrep  return EXITREP;
{DIR}endrep   return ENDREP;
<inhibit>{DIR}rep[^\n]*	;
<inhibit>{DIR}exitrep[^\n]*	;
<inhibit>{DIR}endrep[^\n]*	;

{DIR}if       return IF;
{DIR}elif     return ELIF;
{DIR}else     return ELSE;
{DIR}endif    return ENDIF;
<inhibit>{DIR}if       { BEGIN(INITIAL); return IF; }
<inhibit>{DIR}elif     { BEGIN(INITIAL); return ELIF; }
<inhibit>{DIR}else     { BEGIN(INITIAL); return ELSE; }
<inhibit>{DIR}endif    { BEGIN(INITIAL); return ENDIF; }

    /* test defines */
{DIR}ifdef    return IFDEF;
{DIR}elifdef  return ELIFDEF;
{DIR}ifndef   return IFNDEF;
{DIR}elifndef return ELIFNDEF;
<inhibit>{DIR}ifdef    { BEGIN(INITIAL); return IFDEF; }
<inhibit>{DIR}elifdef  { BEGIN(INITIAL); return ELIFDEF; }
<inhibit>{DIR}ifndef   { BEGIN(INITIAL); return IFNDEF; }
<inhibit>{DIR}elifndef { BEGIN(INITIAL); return ELIFNDEF; }

    /* test context stack */
{DIR}ifctx    return IFCTX;
{DIR}elifctx  return ELIFCTX;
<inhibit>{DIR}ifctx    { BEGIN(INITIAL); return IFCTX; }
<inhibit>{DIR}elifctx  { BEGIN(INITIAL); return ELIFCTX; }

    /* test exact identity */
{DIR}ifidn    return IFIDN;
{DIR}elifidn  return ELIFIDN;
{DIR}ifidni   return IFIDNI;
{DIR}elifidni return ELIFIDNI;
<inhibit>{DIR}ifidn    { BEGIN(INITIAL); return IFIDN; }
<inhibit>{DIR}elifidn  { BEGIN(INITIAL); return ELIFIDN; }
<inhibit>{DIR}ifidni   { BEGIN(INITIAL); return IFIDNI; }
<inhibit>{DIR}elifidni { BEGIN(INITIAL); return ELIFIDNI; }

    /* test token types */
{DIR}ifid     return IFID;
{DIR}elifid   return ELIFID;
{DIR}ifnum    return IFNUM;
{DIR}elifnum  return ELIFNUM;
{DIR}ifstr    return IFSTR;
{DIR}elifstr  return ELIFSTR;
<inhibit>{DIR}ifid     { BEGIN(INITIAL); return IFID; }
<inhibit>{DIR}elifid   { BEGIN(INITIAL); return ELIFID; }
<inhibit>{DIR}ifnum    { BEGIN(INITIAL); return IFNUM; }
<inhibit>{DIR}elifnum  { BEGIN(INITIAL); return ELIFNUM; }
<inhibit>{DIR}ifstr    { BEGIN(INITIAL); return IFSTR; }
<inhibit>{DIR}elifstr  { BEGIN(INITIAL); return ELIFSTR; }

    /* error reporting */
<inhibit>{DIR}error[^\n]*  ;
{DIR}error[ ]+.*    { yylval.str_val = yytext; return ERROR; }

    /* context stack management */
{DIR}push     return PUSH;
{DIR}pop      return POP;
{DIR}repl     return REPL;
<inhibit>{DIR}push[^\n]*  ;
<inhibit>{DIR}pop[^\n]*  ;
<inhibit>{DIR}repl[^\n]*  ;

<inhibit>[^%\n]*\n { yapp_preproc_line_number++; return '\n'; }

;.*\n	{ yapp_preproc_line_number++; return '\n'; }

{WS}+	{ yylval.str_val = yytext; return WHITESPACE; }

{WS}*\n	{ yapp_preproc_line_number++; return '\n'; }


[][+*/,()-] { return yytext[0]; }

<inhibit>.  {
    yasm_warn_set(YASM_WARN_PREPROC, N_("Unhandled character in <inhibit> `%s'"), yasm__conv_unprint(yytext[0]));
}

.	{
    yasm_warn_set(YASM_WARN_PREPROC, N_("ignoring unrecognized character `%s'"),
	    	    yasm__conv_unprint(yytext[0]));
}

%%

void
yapp_lex_initialize(FILE *f)
{
    SLIST_INIT(&includes_head);
    yyin = f;
}

void set_inhibit(void)
{
    BEGIN(inhibit);
}
