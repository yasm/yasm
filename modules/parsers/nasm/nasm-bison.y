/*
 * NASM-compatible bison parser
 *
 *  Copyright (C) 2001  Peter Johnson, Michael Urman
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
%{
#include "util.h"
RCSID("$IdPath$");

#ifdef STDC_HEADERS
# include <math.h>
#endif

#include "bitvect.h"

#include "globals.h"
#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"


#define YYDEBUG 1

void init_table(void);
extern int nasm_parser_lex(void);
void nasm_parser_error(const char *);
static void nasm_parser_directive(const char *name, const char *val);

extern objfmt *nasm_parser_objfmt;
extern sectionhead nasm_parser_sections;
extern section *nasm_parser_cur_section;
extern char *nasm_parser_locallabel_base;

static bytecode *nasm_parser_prev_bc = (bytecode *)NULL;
static bytecode *nasm_parser_temp_bc;

%}

%union {
    unsigned int int_info;
    char *str_val;
    intnum *intn;
    floatnum *flt;
    symrec *sym;
    unsigned char groupdata[4];
    effaddr *ea;
    expr *exp;
    immval *im_val;
    targetval tgt_val;
    datavalhead datahead;
    dataval *data;
    bytecode *bc;
}

%token <intn> INTNUM
%token <flt> FLTNUM
%token <str_val> DIRECTIVE_NAME DIRECTIVE_VAL STRING
%token <int_info> BYTE WORD DWORD QWORD TWORD DQWORD
%token <int_info> DECLARE_DATA
%token <int_info> RESERVE_SPACE
%token INCBIN EQU TIMES
%token SEG WRT NEAR SHORT FAR NOSPLIT ORG
%token TO
%token O16 O32 A16 A32 LOCK REPNZ REP REPZ
%token <int_info> OPERSIZE ADDRSIZE
%token <int_info> CR4 CRREG_NOTCR4 DRREG TRREG ST0 FPUREG_NOTST0 MMXREG XMMREG
%token <int_info> REG_EAX REG_ECX REG_EDX REG_EBX
%token <int_info> REG_ESP REG_EBP REG_ESI REG_EDI
%token <int_info> REG_AX REG_CX REG_DX REG_BX REG_SP REG_BP REG_SI REG_DI
%token <int_info> REG_AL REG_CL REG_DL REG_BL REG_AH REG_CH REG_DH REG_BH
%token <int_info> REG_ES REG_CS REG_SS REG_DS REG_FS REG_GS
%token LEFT_OP RIGHT_OP SIGNDIV SIGNMOD START_SECTION_ID
%token <str_val> ID LOCAL_ID SPECIAL_ID

/* instruction tokens (dynamically generated) */
/* @TOKENS@ */

/* @TYPES@ */

%type <bc> line lineexp exp instr instrbase

%type <int_info> fpureg rawreg32 reg32 rawreg16 reg16 reg8 segreg
%type <ea> mem memaddr memfar
%type <ea> mem8x mem16x mem32x mem64x mem80x mem128x
%type <ea> mem8 mem16 mem32 mem64 mem80 mem128 mem1632
%type <ea> rm8x rm16x rm32x /*rm64x rm128x*/
%type <ea> rm8 rm16 rm32 rm64 rm128
%type <im_val> imm imm8x imm16x imm32x imm8 imm16 imm32
%type <exp> expr expr_no_string memexpr
%type <sym> explabel
%type <str_val> label_id
%type <tgt_val> target
%type <data> dataval
%type <datahead> datavals

%left '|'
%left '^'
%left '&'
%left LEFT_OP RIGHT_OP
%left '-' '+'
%left '*' '/' SIGNDIV '%' SIGNMOD
%nonassoc UNARYOP

%%
input: /* empty */
    | input line    {
	nasm_parser_temp_bc = bytecodes_append(section_get_bytecodes(nasm_parser_cur_section),
					       $2);
	if (nasm_parser_temp_bc)
	    nasm_parser_prev_bc = nasm_parser_temp_bc;
	line_number++;
    }
;

line: '\n'		{ $$ = (bytecode *)NULL; }
    | lineexp '\n'
    | directive '\n'	{ $$ = (bytecode *)NULL; }
    | error '\n'	{
	Error(_("label or instruction expected at start of line"));
	$$ = (bytecode *)NULL;
	yyerrok;
    }
;

lineexp: exp
    | TIMES expr exp			{ $$ = $3; SetBCMultiple($$, $2); }
    | label				{ $$ = (bytecode *)NULL; }
    | label exp				{ $$ = $2; }
    | label TIMES expr exp		{ $$ = $4; SetBCMultiple($$, $3); }
    | label_id EQU expr			{
	symrec_define_equ($1, $3);
	xfree($1);
	$$ = (bytecode *)NULL;
    }
;

exp: instr
    | DECLARE_DATA datavals	    { $$ = bytecode_new_data(&$2, $1); }
    | RESERVE_SPACE expr	    { $$ = bytecode_new_reserve($2, $1); }
;

datavals: dataval	    {
	datavals_initialize(&$$);
	datavals_append(&$$, $1);
    }
    | datavals ',' dataval  {
	datavals_append(&$1, $3);
	$$ = $1;
    }
;

dataval: expr_no_string	{ $$ = dataval_new_expr($1); }
    | STRING		{ $$ = dataval_new_string($1); }
    | error		{
	Error(_("expression syntax error"));
	$$ = (dataval *)NULL;
    }
;

label: label_id	    {
	symrec_define_label($1, nasm_parser_cur_section, nasm_parser_prev_bc,
			    1);
	xfree($1);
    }
    | label_id ':'  {
	symrec_define_label($1, nasm_parser_cur_section, nasm_parser_prev_bc,
			    1);
	xfree($1);
    }
;

label_id: ID	    {
	$$ = $1;
	if (nasm_parser_locallabel_base)
	    xfree(nasm_parser_locallabel_base);
	nasm_parser_locallabel_base = xstrdup($1);
    }
    | SPECIAL_ID
    | LOCAL_ID
;

/* directives */
directive: '[' DIRECTIVE_NAME DIRECTIVE_VAL ']'	{
	nasm_parser_directive($2, $3);
	xfree($2);
	xfree($3);
    }
    | '[' DIRECTIVE_NAME DIRECTIVE_VAL error	{
	Error(_("missing `%c'"), ']');
	xfree($2);
	xfree($3);
    }
    | '[' DIRECTIVE_NAME error			{
	Error(_("missing argument to `%s'"), $2);
	xfree($2);
    }
;

/* register groupings */
fpureg: ST0
    | FPUREG_NOTST0
;

rawreg32: REG_EAX
    | REG_ECX
    | REG_EDX
    | REG_EBX
    | REG_ESP
    | REG_EBP
    | REG_ESI
    | REG_EDI
;

reg32: rawreg32
    | DWORD reg32
;

rawreg16: REG_AX
    | REG_CX
    | REG_DX
    | REG_BX
    | REG_SP
    | REG_BP
    | REG_SI
    | REG_DI
;

reg16: rawreg16
    | WORD reg16
;

reg8: REG_AL
    | REG_CL
    | REG_DL
    | REG_BL
    | REG_AH
    | REG_CH
    | REG_DH
    | REG_BH
    | BYTE reg8
;

segreg:  REG_ES
    | REG_SS
    | REG_DS
    | REG_FS
    | REG_GS
    | REG_CS
    | WORD segreg
;

/* memory addresses */
/* FIXME: Is there any way this redundancy can be eliminated?  This is almost
 * identical to expr: the only difference is that FLTNUM is replaced by
 * rawreg16 and rawreg32.
 *
 * Note that the two can't be just combined because of conflicts caused by imm
 * vs. reg.  I don't see a simple solution right now to this.
 *
 * We don't attempt to check memory expressions for validity here.
 */
memexpr: INTNUM			{ $$ = expr_new_ident(ExprInt($1)); }
    | rawreg16			{ $$ = expr_new_ident(ExprReg($1, 16)); }
    | rawreg32			{ $$ = expr_new_ident(ExprReg($1, 32)); }
    | explabel			{ $$ = expr_new_ident(ExprSym($1)); }
    /*| memexpr '||' memexpr	{ $$ = expr_new_tree($1, EXPR_LOR, $3); }*/
    | memexpr '|' memexpr	{ $$ = expr_new_tree($1, EXPR_OR, $3); }
    | memexpr '^' memexpr	{ $$ = expr_new_tree($1, EXPR_XOR, $3); }
    /*| expr '&&' memexpr	{ $$ = expr_new_tree($1, EXPR_LAND, $3); }*/
    | memexpr '&' memexpr	{ $$ = expr_new_tree($1, EXPR_AND, $3); }
    /*| memexpr '==' memexpr	{ $$ = expr_new_tree($1, EXPR_EQUALS, $3); }*/
    /*| memexpr '>' memexpr	{ $$ = expr_new_tree($1, EXPR_GT, $3); }*/
    /*| memexpr '<' memexpr	{ $$ = expr_new_tree($1, EXPR_GT, $3); }*/
    /*| memexpr '>=' memexpr	{ $$ = expr_new_tree($1, EXPR_GE, $3); }*/
    /*| memexpr '<=' memexpr	{ $$ = expr_new_tree($1, EXPR_GE, $3); }*/
    /*| memexpr '!=' memexpr	{ $$ = expr_new_tree($1, EXPR_NE, $3); }*/
    | memexpr LEFT_OP memexpr	{ $$ = expr_new_tree($1, EXPR_SHL, $3); }
    | memexpr RIGHT_OP memexpr	{ $$ = expr_new_tree($1, EXPR_SHR, $3); }
    | memexpr '+' memexpr	{ $$ = expr_new_tree($1, EXPR_ADD, $3); }
    | memexpr '-' memexpr	{ $$ = expr_new_tree($1, EXPR_SUB, $3); }
    | memexpr '*' memexpr	{ $$ = expr_new_tree($1, EXPR_MUL, $3); }
    | memexpr '/' memexpr	{ $$ = expr_new_tree($1, EXPR_DIV, $3); }
    | memexpr SIGNDIV memexpr	{ $$ = expr_new_tree($1, EXPR_SIGNDIV, $3); }
    | memexpr '%' memexpr	{ $$ = expr_new_tree($1, EXPR_MOD, $3); }
    | memexpr SIGNMOD memexpr	{ $$ = expr_new_tree($1, EXPR_SIGNMOD, $3); }
    | '+' memexpr %prec UNARYOP	{ $$ = $2; }
    | '-' memexpr %prec UNARYOP	{ $$ = expr_new_branch(EXPR_NEG, $2); }
    /*| '!' memexpr		{ $$ = expr_new_branch(EXPR_LNOT, $2); }*/
    | '~' memexpr %prec UNARYOP	{ $$ = expr_new_branch(EXPR_NOT, $2); }
    | '(' memexpr ')'		{ $$ = $2; }
    | STRING			{
	$$ = expr_new_ident(ExprInt(intnum_new_charconst_nasm($1)));
	xfree($1);
    }
    | error			{ Error(_("invalid effective address")); }
;

memaddr: memexpr	    { $$ = effaddr_new_expr($1); SetEASegment($$, 0); }
    | REG_CS ':' memaddr    { $$ = $3; SetEASegment($$, 0x2E); }
    | REG_SS ':' memaddr    { $$ = $3; SetEASegment($$, 0x36); }
    | REG_DS ':' memaddr    { $$ = $3; SetEASegment($$, 0x3E); }
    | REG_ES ':' memaddr    { $$ = $3; SetEASegment($$, 0x26); }
    | REG_FS ':' memaddr    { $$ = $3; SetEASegment($$, 0x64); }
    | REG_GS ':' memaddr    { $$ = $3; SetEASegment($$, 0x65); }
    | BYTE memaddr	    { $$ = $2; SetEALen($$, 1); }
    | WORD memaddr	    { $$ = $2; SetEALen($$, 2); }
    | DWORD memaddr	    { $$ = $2; SetEALen($$, 4); }
    | NOSPLIT memaddr	    { $$ = $2; SetEANosplit($$, 1); }
;

mem: '[' memaddr ']'	{ $$ = $2; }
;

/* explicit memory */
mem8x: BYTE mem		{ $$ = $2; }
;
mem16x: WORD mem	{ $$ = $2; }
;
mem32x: DWORD mem	{ $$ = $2; }
;
mem64x: QWORD mem	{ $$ = $2; }
;
mem80x: TWORD mem	{ $$ = $2; }
;
mem128x: DQWORD mem	{ $$ = $2; }
;

/* FAR memory, for jmp and call */
memfar: FAR mem		{ $$ = $2; }
;

/* implicit memory */
mem8: mem
    | mem8x
;
mem16: mem
    | mem16x
;
mem32: mem
    | mem32x
;
mem64: mem
    | mem64x
;
mem80: mem
    | mem80x
;
mem128: mem
    | mem128x
;

/* both 16 and 32 bit memory */
mem1632: mem
    | mem16x
    | mem32x
;

/* explicit register or memory */
rm8x: reg8	{ $$ = effaddr_new_reg($1); }
    | mem8x
;
rm16x: reg16	{ $$ = effaddr_new_reg($1); }
    | mem16x
;
rm32x: reg32	{ $$ = effaddr_new_reg($1); }
    | mem32x
;
/* not needed:
rm64x: MMXREG	{ $$ = effaddr_new_reg($1); }
    | mem64x
;
rm128x: XMMREG	{ $$ = effaddr_new_reg($1); }
    | mem128x
;
*/

/* implicit register or memory */
rm8: reg8	{ $$ = effaddr_new_reg($1); }
    | mem8
;
rm16: reg16	{ $$ = effaddr_new_reg($1); }
    | mem16
;
rm32: reg32	{ $$ = effaddr_new_reg($1); }
    | mem32
;
rm64: MMXREG	{ $$ = effaddr_new_reg($1); }
    | mem64
;
rm128: XMMREG	{ $$ = effaddr_new_reg($1); }
    | mem128
;

/* immediate values */
imm: expr   { $$ = immval_new_expr($1); }
;

/* explicit immediates */
imm8x: BYTE imm	    { $$ = $2; }
;
imm16x: WORD imm    { $$ = $2; }
;
imm32x: DWORD imm   { $$ = $2; }
;

/* implicit immediates */
imm8: imm
    | imm8x
;
imm16: imm
    | imm16x
;
imm32: imm
    | imm32x
;

/* jump targets */
target: expr		{ $$.val = $1; SetOpcodeSel(&$$.op_sel, JR_NONE); }
    | SHORT target	{ $$ = $2; SetOpcodeSel(&$$.op_sel, JR_SHORT_FORCED); }
    | NEAR target	{ $$ = $2; SetOpcodeSel(&$$.op_sel, JR_NEAR_FORCED); }
;

/* expression trees */
expr_no_string: INTNUM		{ $$ = expr_new_ident(ExprInt($1)); }
    | FLTNUM			{ $$ = expr_new_ident(ExprFloat($1)); }
    | explabel			{ $$ = expr_new_ident(ExprSym($1)); }
    /*| expr '||' expr		{ $$ = expr_new_tree($1, EXPR_LOR, $3); }*/
    | expr '|' expr		{ $$ = expr_new_tree($1, EXPR_OR, $3); }
    | expr '^' expr		{ $$ = expr_new_tree($1, EXPR_XOR, $3); }
    /*| expr '&&' expr		{ $$ = expr_new_tree($1, EXPR_LAND, $3); }*/
    | expr '&' expr		{ $$ = expr_new_tree($1, EXPR_AND, $3); }
    /*| expr '==' expr		{ $$ = expr_new_tree($1, EXPR_EQUALS, $3); }*/
    /*| expr '>' expr		{ $$ = expr_new_tree($1, EXPR_GT, $3); }*/
    /*| expr '<' expr		{ $$ = expr_new_tree($1, EXPR_GT, $3); }*/
    /*| expr '>=' expr		{ $$ = expr_new_tree($1, EXPR_GE, $3); }*/
    /*| expr '<=' expr		{ $$ = expr_new_tree($1, EXPR_GE, $3); }*/
    /*| expr '!=' expr		{ $$ = expr_new_tree($1, EXPR_NE, $3); }*/
    | expr LEFT_OP expr		{ $$ = expr_new_tree($1, EXPR_SHL, $3); }
    | expr RIGHT_OP expr	{ $$ = expr_new_tree($1, EXPR_SHR, $3); }
    | expr '+' expr		{ $$ = expr_new_tree($1, EXPR_ADD, $3); }
    | expr '-' expr		{ $$ = expr_new_tree($1, EXPR_SUB, $3); }
    | expr '*' expr		{ $$ = expr_new_tree($1, EXPR_MUL, $3); }
    | expr '/' expr		{ $$ = expr_new_tree($1, EXPR_DIV, $3); }
    | expr SIGNDIV expr		{ $$ = expr_new_tree($1, EXPR_SIGNDIV, $3); }
    | expr '%' expr		{ $$ = expr_new_tree($1, EXPR_MOD, $3); }
    | expr SIGNMOD expr		{ $$ = expr_new_tree($1, EXPR_SIGNMOD, $3); }
    | '+' expr %prec UNARYOP	{ $$ = $2; }
    | '-' expr %prec UNARYOP	{ $$ = expr_new_branch(EXPR_NEG, $2); }
    /*| '!' expr		{ $$ = expr_new_branch(EXPR_LNOT, $2); }*/
    | '~' expr %prec UNARYOP	{ $$ = expr_new_branch(EXPR_NOT, $2); }
    | '(' expr ')'		{ $$ = $2; }
;

expr: expr_no_string
    | STRING		{
	$$ = expr_new_ident(ExprInt(intnum_new_charconst_nasm($1)));
	xfree($1);
    }
;

explabel: ID		{ $$ = symrec_use($1); xfree($1); }
    | SPECIAL_ID	{ $$ = symrec_use($1); xfree($1); }
    | LOCAL_ID		{ $$ = symrec_use($1); xfree($1); }
    | '$'		{
	$$ = symrec_define_label("$", nasm_parser_cur_section,
				 nasm_parser_prev_bc, 0);
    }
    | START_SECTION_ID	{
	$$ = symrec_use(section_get_name(nasm_parser_cur_section));
    }
;

instr: instrbase
    | OPERSIZE instr	{ $$ = $2; SetInsnOperSizeOverride($$, $1); }
    | ADDRSIZE instr	{ $$ = $2; SetInsnAddrSizeOverride($$, $1); }
    | REG_CS instr	{ $$ = $2; SetEASegment(GetInsnEA($$), 0x2E); }
    | REG_SS instr	{ $$ = $2; SetEASegment(GetInsnEA($$), 0x36); }
    | REG_DS instr	{ $$ = $2; SetEASegment(GetInsnEA($$), 0x3E); }
    | REG_ES instr	{ $$ = $2; SetEASegment(GetInsnEA($$), 0x26); }
    | REG_FS instr	{ $$ = $2; SetEASegment(GetInsnEA($$), 0x64); }
    | REG_GS instr	{ $$ = $2; SetEASegment(GetInsnEA($$), 0x65); }
    | LOCK instr	{ $$ = $2; SetInsnLockRepPrefix($$, 0xF0); }
    | REPNZ instr	{ $$ = $2; SetInsnLockRepPrefix($$, 0xF2); }
    | REP instr		{ $$ = $2; SetInsnLockRepPrefix($$, 0xF3); }
    | REPZ instr	{ $$ = $2; SetInsnLockRepPrefix($$, 0xF4); }
;

/* instruction grammars (dynamically generated) */
/* @INSTRUCTIONS@ */

%%

static void
nasm_parser_directive(const char *name, const char *val)
{
    long lval;
    char *end;

    if (strcasecmp(name, "section") == 0) {
	nasm_parser_cur_section = sections_switch(&nasm_parser_sections,
						  nasm_parser_objfmt, val);
	nasm_parser_prev_bc = (bytecode *)NULL;
	symrec_define_label(val, nasm_parser_cur_section, (bytecode *)NULL, 1);
    } else if (strcasecmp(name, "bits") == 0) {
	lval = strtol(val, &end, 10);
	if (*val == '\0' || *end != '\0' || (lval != 16 && lval != 32))
	    Error(_("`%s' is not a valid argument to [BITS]"), val);
	else
	    mode_bits = (unsigned char)lval;
    } else {
	printf("Directive: Name=`%s' Value=`%s'\n", name, val);
    }
}

void
nasm_parser_error(const char *s)
{
    ParserError(s);
}

