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

#include "arch.h"

#include "src/parsers/nasm/nasm-defs.h"

void init_table(void);
extern int nasm_parser_lex(void);
void nasm_parser_error(const char *);
static void nasm_parser_directive(const char *name,
				  valparamhead *valparams,
				  /*@null@*/ valparamhead *objext_valparams);

extern objfmt *nasm_parser_objfmt;
extern sectionhead nasm_parser_sections;
extern section *nasm_parser_cur_section;
extern char *nasm_parser_locallabel_base;

static /*@null@*/ bytecode *nasm_parser_prev_bc = (bytecode *)NULL;
static bytecode *nasm_parser_temp_bc;

/* additional data declarations (dynamically generated) */
/* @DATADECLS@ */

/*@-usedef -nullassign -memtrans -usereleased -compdef -mustfree@*/
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
    x86_targetval tgt_val;
    datavalhead datahead;
    dataval *data;
    bytecode *bc;
    valparamhead dir_valparams;
    valparam *dir_valparam;
}

%token <intn> INTNUM
%token <flt> FLTNUM
%token <str_val> DIRECTIVE_NAME STRING FILENAME
%token <int_info> BYTE WORD DWORD QWORD TWORD DQWORD
%token <int_info> DECLARE_DATA
%token <int_info> RESERVE_SPACE
%token INCBIN EQU TIMES
%token SEG WRT NEAR SHORT FAR NOSPLIT ORG
%token TO
%token LOCK REPNZ REP REPZ
%token <int_info> OPERSIZE ADDRSIZE
%token <int_info> CR4 CRREG_NOTCR4 DRREG TRREG ST0 FPUREG_NOTST0 MMXREG XMMREG
%token <int_info> REG_EAX REG_ECX REG_EDX REG_EBX
%token <int_info> REG_ESP REG_EBP REG_ESI REG_EDI
%token <int_info> REG_AX REG_CX REG_DX REG_BX REG_SP REG_BP REG_SI REG_DI
%token <int_info> REG_AL REG_CL REG_DL REG_BL REG_AH REG_CH REG_DH REG_BH
%token <int_info> REG_ES REG_CS REG_SS REG_DS REG_FS REG_GS
%token LEFT_OP RIGHT_OP SIGNDIV SIGNMOD START_SECTION_ID
%token <str_val> ID LOCAL_ID SPECIAL_ID
%token LINE

/* instruction tokens (dynamically generated) */
/* @TOKENS@ */

/* @TYPES@ */

%type <bc> line lineexp exp instr instrbase

%type <int_info> reg_eax reg_ecx
%type <int_info> reg_ax reg_cx reg_dx
%type <int_info> reg_al reg_cl
%type <int_info> reg_es reg_cs reg_ss reg_ds reg_fs reg_gs
%type <int_info> fpureg rawreg32 reg32 rawreg16 reg16 reg8 segreg
%type <ea> mem memaddr memfar
%type <ea> mem8x mem16x mem32x mem64x mem80x mem128x
%type <ea> mem8 mem16 mem32 mem64 mem80 mem128 mem1632
%type <ea> rm8x rm16x rm32x /*rm64x rm128x*/
%type <ea> rm8 rm16 rm32 rm64 rm128
%type <im_val> imm imm8x imm16x imm32x imm8 imm16 imm32
%type <exp> expr expr_no_string memexpr direxpr
%type <sym> explabel
%type <str_val> label_id
%type <tgt_val> target
%type <data> dataval
%type <datahead> datavals
%type <dir_valparams> directive_valparams
%type <dir_valparam> directive_valparam

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
	nasm_parser_temp_bc = bcs_append(section_get_bytecodes(nasm_parser_cur_section),
					       $2);
	if (nasm_parser_temp_bc)
	    nasm_parser_prev_bc = nasm_parser_temp_bc;
	line_number += line_number_inc;
    }
;

line: '\n'		{ $$ = (bytecode *)NULL; }
    | lineexp '\n'
    | LINE INTNUM '+' INTNUM FILENAME '\n' {
	line_number = (unsigned int)intnum_get_uint($2);
	line_number_inc = (unsigned int)intnum_get_uint($4);
	line_number -= line_number_inc;	/* as we'll add it back in */
	switch_filename($5);
	intnum_delete($2);
	intnum_delete($4);
	xfree($5);
	$$ = (bytecode *)NULL;
    }
    | directive '\n'	{ $$ = (bytecode *)NULL; }
    | error '\n'	{
	Error(_("label or instruction expected at start of line"));
	$$ = (bytecode *)NULL;
	yyerrok;
    }
;

lineexp: exp
    | TIMES expr exp			{ $$ = $3; bc_set_multiple($$, $2); }
    | label				{ $$ = (bytecode *)NULL; }
    | label exp				{ $$ = $2; }
    | label TIMES expr exp		{ $$ = $4; bc_set_multiple($$, $3); }
    | label_id EQU expr			{
	symrec_define_equ($1, $3);
	xfree($1);
	$$ = (bytecode *)NULL;
    }
;

exp: instr
    | DECLARE_DATA datavals		{ $$ = bc_new_data(&$2, $1); }
    | RESERVE_SPACE expr		{ $$ = bc_new_reserve($2, $1); }
    | INCBIN STRING			{ $$ = bc_new_incbin($2, NULL, NULL); }
    | INCBIN STRING ',' expr		{ $$ = bc_new_incbin($2, $4, NULL); }
    | INCBIN STRING ',' expr ',' expr	{ $$ = bc_new_incbin($2, $4, $6); }
;

datavals: dataval	    { dvs_initialize(&$$); dvs_append(&$$, $1); }
    | datavals ',' dataval  { dvs_append(&$1, $3); $$ = $1; }
;

dataval: expr_no_string	{ $$ = dv_new_expr($1); }
    | STRING		{ $$ = dv_new_string($1); }
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
directive: '[' DIRECTIVE_NAME directive_val ']'	{
	xfree($2);
    }
    | '[' DIRECTIVE_NAME error ']'		{
	Error(_("invalid arguments to [%s]"), $2);
	xfree($2);
    }
;

    /* $<str_val>0 is the DIRECTIVE_NAME */
    /* After : is (optional) object-format specific extension */
directive_val: directive_valparams {
	nasm_parser_directive($<str_val>0, &$1, NULL);
    }
    | directive_valparams ':' directive_valparams {
	nasm_parser_directive($<str_val>0, &$1, &$3);
    }
;

directive_valparams: directive_valparam		{
	vps_initialize(&$$);
	vps_append(&$$, $1);
    }
    | directive_valparams directive_valparam	{
	vps_append(&$1, $2);
	$$ = $1;
    }
;

directive_valparam: direxpr	{
	/* If direxpr is just an ID, put it in val and delete the expr */
	const /*@null@*/ symrec *vp_symrec;
	if ((vp_symrec = expr_get_symrec(&$1, 0))) {
	    vp_new($$, xstrdup(symrec_get_name(vp_symrec)), NULL);
	    expr_delete($1);
	} else
	    vp_new($$, NULL, $1);
    }
    | ID '=' direxpr		{ vp_new($$, $1, $3); }
;

/* register groupings */
fpureg: ST0
    | FPUREG_NOTST0
;

reg_eax: REG_EAX
    | DWORD reg_eax	{ $$ = $2; }
;

reg_ecx: REG_ECX
    | DWORD reg_ecx	{ $$ = $2; }
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
    | DWORD reg32	{ $$ = $2; }
;

reg_ax: REG_AX
    | WORD reg_ax	{ $$ = $2; }
;

reg_cx: REG_CX
    | WORD reg_cx	{ $$ = $2; }
;

reg_dx: REG_DX
    | WORD reg_dx	{ $$ = $2; }
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
    | WORD reg16	{ $$ = $2; }
;

reg_al: REG_AL
    | BYTE reg_al	{ $$ = $2; }
;

reg_cl: REG_CL
    | BYTE reg_cl	{ $$ = $2; }
;

reg8: REG_AL
    | REG_CL
    | REG_DL
    | REG_BL
    | REG_AH
    | REG_CH
    | REG_DH
    | REG_BH
    | BYTE reg8		{ $$ = $2; }
;

reg_es: REG_ES
    | WORD reg_es	{ $$ = $2; }
;

reg_ss: REG_SS
    | WORD reg_ss	{ $$ = $2; }
;

reg_ds: REG_DS
    | WORD reg_ds	{ $$ = $2; }
;

reg_fs: REG_FS
    | WORD reg_fs	{ $$ = $2; }
;

reg_gs: REG_GS
    | WORD reg_gs	{ $$ = $2; }
;

reg_cs: REG_CS
    | WORD reg_cs	{ $$ = $2; }
;

segreg: REG_ES
    | REG_SS
    | REG_DS
    | REG_FS
    | REG_GS
    | REG_CS
    | WORD segreg	{ $$ = $2; }
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

memaddr: memexpr	    {
	$$ = x86_ea_new_expr($1);
	x86_ea_set_segment($$, 0);
    }
    | REG_CS ':' memaddr    { $$ = $3; x86_ea_set_segment($$, 0x2E); }
    | REG_SS ':' memaddr    { $$ = $3; x86_ea_set_segment($$, 0x36); }
    | REG_DS ':' memaddr    { $$ = $3; x86_ea_set_segment($$, 0x3E); }
    | REG_ES ':' memaddr    { $$ = $3; x86_ea_set_segment($$, 0x26); }
    | REG_FS ':' memaddr    { $$ = $3; x86_ea_set_segment($$, 0x64); }
    | REG_GS ':' memaddr    { $$ = $3; x86_ea_set_segment($$, 0x65); }
    | BYTE memaddr	    { $$ = $2; ea_set_len($$, 1); }
    | WORD memaddr	    { $$ = $2; ea_set_len($$, 2); }
    | DWORD memaddr	    { $$ = $2; ea_set_len($$, 4); }
    | NOSPLIT memaddr	    { $$ = $2; ea_set_nosplit($$, 1); }
;

mem: '[' memaddr ']'	{ $$ = $2; }
;

/* explicit memory */
mem8x: BYTE mem		{ $$ = $2; }
    | BYTE mem8x	{ $$ = $2; }
;
mem16x: WORD mem	{ $$ = $2; }
    | WORD mem16x	{ $$ = $2; }
;
mem32x: DWORD mem	{ $$ = $2; }
    | DWORD mem32x	{ $$ = $2; }
;
mem64x: QWORD mem	{ $$ = $2; }
    | QWORD mem64x	{ $$ = $2; }
;
mem80x: TWORD mem	{ $$ = $2; }
    | TWORD mem80x	{ $$ = $2; }
;
mem128x: DQWORD mem	{ $$ = $2; }
    | DQWORD mem128x	{ $$ = $2; }
;

/* FAR memory, for jmp and call */
memfar: FAR mem		{ $$ = $2; }
    | FAR memfar	{ $$ = $2; }
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
rm8x: reg8	{ $$ = x86_ea_new_reg($1); }
    | mem8x
;
rm16x: reg16	{ $$ = x86_ea_new_reg($1); }
    | mem16x
;
rm32x: reg32	{ $$ = x86_ea_new_reg($1); }
    | mem32x
;
/* not needed:
rm64x: MMXREG	{ $$ = x86_ea_new_reg($1); }
    | mem64x
;
rm128x: XMMREG	{ $$ = x86_ea_new_reg($1); }
    | mem128x
;
*/

/* implicit register or memory */
rm8: reg8	{ $$ = x86_ea_new_reg($1); }
    | mem8
;
rm16: reg16	{ $$ = x86_ea_new_reg($1); }
    | mem16
;
rm32: reg32	{ $$ = x86_ea_new_reg($1); }
    | mem32
;
rm64: MMXREG	{ $$ = x86_ea_new_reg($1); }
    | mem64
;
rm128: XMMREG	{ $$ = x86_ea_new_reg($1); }
    | mem128
;

/* immediate values */
imm: expr   { $$ = imm_new_expr($1); }
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
target: expr		{
	$$.val = $1;
	x86_set_jmprel_opcode_sel(&$$.op_sel, JR_NONE);
    }
    | SHORT target	{
	$$ = $2;
	x86_set_jmprel_opcode_sel(&$$.op_sel, JR_SHORT_FORCED);
    }
    | NEAR target	{
	$$ = $2;
	x86_set_jmprel_opcode_sel(&$$.op_sel, JR_NEAR_FORCED);
    }
;

/* expression trees */

/* expr w/o FLTNUM and unary + and -, for use in directives */
direxpr: INTNUM			{ $$ = expr_new_ident(ExprInt($1)); }
    | ID			{
	$$ = expr_new_ident(ExprSym(symrec_define_label($1, NULL, NULL, 0)));
	xfree($1);
    }
    | direxpr '|' direxpr	{ $$ = expr_new_tree($1, EXPR_OR, $3); }
    | direxpr '^' direxpr	{ $$ = expr_new_tree($1, EXPR_XOR, $3); }
    | direxpr '&' direxpr	{ $$ = expr_new_tree($1, EXPR_AND, $3); }
    | direxpr LEFT_OP direxpr	{ $$ = expr_new_tree($1, EXPR_SHL, $3); }
    | direxpr RIGHT_OP direxpr	{ $$ = expr_new_tree($1, EXPR_SHR, $3); }
    | direxpr '+' direxpr	{ $$ = expr_new_tree($1, EXPR_ADD, $3); }
    | direxpr '-' direxpr	{ $$ = expr_new_tree($1, EXPR_SUB, $3); }
    | direxpr '*' direxpr	{ $$ = expr_new_tree($1, EXPR_MUL, $3); }
    | direxpr '/' direxpr	{ $$ = expr_new_tree($1, EXPR_DIV, $3); }
    | direxpr SIGNDIV direxpr	{ $$ = expr_new_tree($1, EXPR_SIGNDIV, $3); }
    | direxpr '%' direxpr	{ $$ = expr_new_tree($1, EXPR_MOD, $3); }
    | direxpr SIGNMOD direxpr	{ $$ = expr_new_tree($1, EXPR_SIGNMOD, $3); }
    /*| '!' expr		{ $$ = expr_new_branch(EXPR_LNOT, $2); }*/
    | '~' direxpr %prec UNARYOP	{ $$ = expr_new_branch(EXPR_NOT, $2); }
    | '(' direxpr ')'		{ $$ = $2; }
;

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

explabel: ID		{
	$$ = symrec_use($1);
	xfree($1);
    }
    | SPECIAL_ID	{
	$$ = symrec_use($1);
	xfree($1);
    }
    | LOCAL_ID		{
	$$ = symrec_use($1);
	xfree($1);
    }
    | '$'		{
	$$ = symrec_define_label("$", nasm_parser_cur_section,
				 nasm_parser_prev_bc, 0);
    }
    | START_SECTION_ID	{
	if (section_is_absolute(nasm_parser_cur_section)) {
	    Error(_("`$$' is not valid within an ABSOLUTE section"));
	    YYERROR;
	} else {
	    const char *ss_name = section_get_name(nasm_parser_cur_section);
	    assert(ss_name != NULL);
	    $$ = symrec_use(ss_name);
	}
    }
;

instr: /* empty */	{
	idata.opersize=0; idata.op_len=0; idata.ea=NULL; idata.imm=NULL;
	$$ = x86_bc_new_insn(&idata);
    }
    | instrbase
    | OPERSIZE instr	{ $$ = $2; x86_bc_insn_opersize_override($$, $1); }
    | ADDRSIZE instr	{ $$ = $2; x86_bc_insn_addrsize_override($$, $1); }
    | REG_CS instr	{
	$$ = $2;
	x86_ea_set_segment(x86_bc_insn_get_ea($$), 0x2E);
    }
    | REG_SS instr	{
	$$ = $2;
	x86_ea_set_segment(x86_bc_insn_get_ea($$), 0x36);
    }
    | REG_DS instr	{
	$$ = $2;
	x86_ea_set_segment(x86_bc_insn_get_ea($$), 0x3E);
    }
    | REG_ES instr	{
	$$ = $2;
	x86_ea_set_segment(x86_bc_insn_get_ea($$), 0x26);
    }
    | REG_FS instr	{
	$$ = $2;
	x86_ea_set_segment(x86_bc_insn_get_ea($$), 0x64);
    }
    | REG_GS instr	{
	$$ = $2;
	x86_ea_set_segment(x86_bc_insn_get_ea($$), 0x65);
    }
    | LOCK instr	{ $$ = $2; x86_bc_insn_set_lockrep_prefix($$, 0xF0); }
    | REPNZ instr	{ $$ = $2; x86_bc_insn_set_lockrep_prefix($$, 0xF2); }
    | REP instr		{ $$ = $2; x86_bc_insn_set_lockrep_prefix($$, 0xF3); }
    | REPZ instr	{ $$ = $2; x86_bc_insn_set_lockrep_prefix($$, 0xF4); }
;

/* instruction grammars (dynamically generated) */
/* @INSTRUCTIONS@ */

%%
/*@=usedef =nullassign =memtrans =usereleased =compdef =mustfree@*/

static void
nasm_parser_directive(const char *name, valparamhead *valparams,
		      valparamhead *objext_valparams)
{
    valparam *vp, *vp2;
    const intnum *intn;
    long lval;

    assert(cur_objfmt != NULL);

    /* Handle (mostly) output-format independent directives here */
    if (strcasecmp(name, "extern") == 0) {
	vp = vps_first(valparams);
	if (vp->val)
	    symrec_declare(vp->val, SYM_EXTERN,
			   cur_objfmt->extern_data_new(vp->val,
						       objext_valparams));
	else
	    Error(_("invalid argument to [%s]"), "EXTERN");
    } else if (strcasecmp(name, "global") == 0) {
	vp = vps_first(valparams);
	if (vp->val)
	    symrec_declare(vp->val, SYM_GLOBAL,
			   cur_objfmt->global_data_new(vp->val,
						       objext_valparams));
	else
	    Error(_("invalid argument to [%s]"), "GLOBAL");
    } else if (strcasecmp(name, "common") == 0) {
	vp = vps_first(valparams);
	if (vp->val) {
	    vp2 = vps_next(vp);
	    if (!vp2 || (!vp2->val && !vp2->param))
		Error(_("no size specified in %s declaration"), "COMMON");
	    else {
		if (vp2->val)
		    symrec_declare(vp->val, SYM_COMMON,
			cur_objfmt->common_data_new(vp->val,
			    expr_new_ident(ExprSym(symrec_use(vp2->val))),
			    objext_valparams));
		else if (vp2->param) {
		    symrec_declare(vp->val, SYM_COMMON,
			cur_objfmt->common_data_new(vp->val, vp2->param,
						    objext_valparams));
		    vp2->param = NULL;
		}
	    }
	} else
	    Error(_("invalid argument to [%s]"), "COMMON");
    } else if (strcasecmp(name, "section") == 0 ||
	       strcasecmp(name, "segment") == 0) {
	section *new_section =
	    cur_objfmt->sections_switch(&nasm_parser_sections, valparams,
					objext_valparams);
	if (new_section) {
	    nasm_parser_cur_section = new_section;
	    nasm_parser_prev_bc = (bytecode *)NULL;
	} else
	    Error(_("invalid argument to [%s]"), "SECTION");
    } else if (strcasecmp(name, "absolute") == 0) {
	/* it can be just an ID or a complete expression, so handle both. */
	vp = vps_first(valparams);
	if (vp->val)
	    nasm_parser_cur_section =
		sections_switch_absolute(&nasm_parser_sections,
		    expr_new_ident(ExprSym(symrec_use(vp->val))));
	else if (vp->param) {
	    nasm_parser_cur_section =
		sections_switch_absolute(&nasm_parser_sections, vp->param);
	    vp->param = NULL;
	}
	nasm_parser_prev_bc = (bytecode *)NULL;
    } else if (strcasecmp(name, "bits") == 0) {
	if ((vp = vps_first(valparams)) && !vp->val && vp->param != NULL &&
	    (intn = expr_get_intnum(&vp->param)) != NULL &&
	    (lval = intnum_get_int(intn)) && (lval == 16 || lval == 32))
	    x86_mode_bits = (unsigned char)lval;
	else
	    Error(_("invalid argument to [%s]"), "BITS");
    } else if (cur_objfmt->directive(name, valparams, objext_valparams)) {
	Error(_("unrecognized directive [%s]"), name);
    }

    vps_delete(valparams);
    if (objext_valparams)
	vps_delete(objext_valparams);
}

void
nasm_parser_error(const char *s)
{
    ParserError(s);
}

