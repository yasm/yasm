/*
 * NASM-compatible bison parser
 *
 *  Copyright (C) 2001  Peter Johnson, Michael Urman
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
#include "util.h"
RCSID("$IdPath$");

#ifdef STDC_HEADERS
# include <math.h>
#endif

#include "bitvect.h"

#include "linemgr.h"
#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"
#include "expr-int.h"
#include "symrec.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"

#include "arch.h"

#include "src/parsers/nasm/nasm-parser.h"
#include "src/parsers/nasm/nasm-defs.h"


static void nasm_parser_error(const char *);
static void nasm_parser_directive
    (const char *name, yasm_valparamhead *valparams,
     /*@null@*/ yasm_valparamhead *objext_valparams);
static int fix_directive_symrec(/*@null@*/ yasm_expr__item *ei,
				/*@null@*/ void *d);

static /*@null@*/ yasm_bytecode *nasm_parser_prev_bc = (yasm_bytecode *)NULL;
static yasm_bytecode *nasm_parser_temp_bc;

/* additional data declarations (dynamically generated) */
/* @DATADECLS@ */

/*@-usedef -nullassign -memtrans -usereleased -compdef -mustfree@*/
%}

%union {
    unsigned int int_info;
    char *str_val;
    yasm_intnum *intn;
    yasm_floatnum *flt;
    yasm_symrec *sym;
    unsigned long arch_data[4];
    yasm_effaddr *ea;
    yasm_expr *exp;
    yasm_datavalhead datahead;
    yasm_dataval *data;
    yasm_bytecode *bc;
    yasm_valparamhead dir_valparams;
    yasm_valparam *dir_valparam;
    struct {
	yasm_insn_operandhead operands;
	int num_operands;
    } insn_operands;
    yasm_insn_operand *insn_operand;
}

%token <intn> INTNUM
%token <flt> FLTNUM
%token <str_val> DIRECTIVE_NAME STRING FILENAME
%token <int_info> BYTE WORD DWORD QWORD TWORD DQWORD
%token <int_info> DECLARE_DATA
%token <int_info> RESERVE_SPACE
%token INCBIN EQU TIMES
%token SEG WRT NOSPLIT
%token <arch_data> INSN PREFIX REG SEGREG TARGETMOD
%token LEFT_OP RIGHT_OP SIGNDIV SIGNMOD START_SECTION_ID
%token <str_val> ID LOCAL_ID SPECIAL_ID
%token LINE

%type <bc> line lineexp exp instr

%type <ea> memaddr
%type <exp> dvexpr expr direxpr
%type <sym> explabel
%type <str_val> label_id label_id_equ
%type <data> dataval
%type <datahead> datavals
%type <dir_valparams> directive_valparams
%type <dir_valparam> directive_valparam
%type <insn_operands> operands
%type <insn_operand> operand

%left '|'
%left '^'
%left '&'
%left LEFT_OP RIGHT_OP
%left '-' '+'
%left '*' '/' SIGNDIV '%' SIGNMOD
%nonassoc UNARYOP
%nonassoc SEG WRT
%left ':'

%%
input: /* empty */
    | input line    {
	nasm_parser_temp_bc =
	    yasm_bcs_append(yasm_section_get_bytecodes(nasm_parser_cur_section),
			    $2);
	if (nasm_parser_temp_bc)
	    nasm_parser_prev_bc = nasm_parser_temp_bc;
	nasm_parser_linemgr->goto_next();
    }
;

line: '\n'		{ $$ = (yasm_bytecode *)NULL; }
    | lineexp '\n'
    | LINE INTNUM '+' INTNUM FILENAME '\n' {
	/* %line indicates the line number of the *next* line, so subtract out
	 * the increment when setting the line number.
	 */
	nasm_parser_linemgr->set($5, yasm_intnum_get_uint($2)
				 - yasm_intnum_get_uint($4),
				 yasm_intnum_get_uint($4));
	yasm_intnum_delete($2);
	yasm_intnum_delete($4);
	xfree($5);
	$$ = (yasm_bytecode *)NULL;
    }
    | '[' { nasm_parser_set_directive_state(); } directive ']' '\n' {
	$$ = (yasm_bytecode *)NULL;
    }
    | error '\n'	{
	cur_we->error(cur_lindex,
		      N_("label or instruction expected at start of line"));
	$$ = (yasm_bytecode *)NULL;
	yyerrok;
    }
;

lineexp: exp
    | TIMES expr exp		{ $$ = $3; yasm_bc_set_multiple($$, $2); }
    | label			{ $$ = (yasm_bytecode *)NULL; }
    | label exp			{ $$ = $2; }
    | label TIMES expr exp	{ $$ = $4; yasm_bc_set_multiple($$, $3); }
    | label_id_equ EQU expr	{
	yasm_symrec_define_equ($1, $3, cur_lindex);
	xfree($1);
	$$ = (yasm_bytecode *)NULL;
    }
;

exp: instr
    | DECLARE_DATA datavals		{
	$$ = yasm_bc_new_data(&$2, $1, cur_lindex);
    }
    | RESERVE_SPACE expr		{
	$$ = yasm_bc_new_reserve($2, $1, cur_lindex);
    }
    | INCBIN STRING			{
	$$ = yasm_bc_new_incbin($2, NULL, NULL, cur_lindex);
    }
    | INCBIN STRING ',' expr		{
	$$ = yasm_bc_new_incbin($2, $4, NULL, cur_lindex);
    }
    | INCBIN STRING ',' expr ',' expr	{
	$$ = yasm_bc_new_incbin($2, $4, $6, cur_lindex);
    }
;

instr: INSN		{
	$$ = nasm_parser_arch->parse.new_insn($1, 0, NULL,
					      nasm_parser_cur_section,
					      nasm_parser_prev_bc, cur_lindex);
    }
    | INSN operands	{
	$$ = nasm_parser_arch->parse.new_insn($1, $2.num_operands,
					      &$2.operands,
					      nasm_parser_cur_section,
					      nasm_parser_prev_bc, cur_lindex);
	yasm_ops_delete(&$2.operands, 0);
    }
    | INSN error	{
	cur_we->error(cur_lindex, N_("expression syntax error"));
	$$ = NULL;
    }
    | PREFIX instr	{
	$$ = $2;
	nasm_parser_arch->parse.handle_prefix($$, $1, cur_lindex);
    }
    | SEGREG instr	{
	$$ = $2;
	nasm_parser_arch->parse.handle_seg_prefix($$, $1[0], cur_lindex);
    }
;

datavals: dataval	    {
	yasm_dvs_initialize(&$$);
	yasm_dvs_append(&$$, $1);
    }
    | datavals ',' dataval  { yasm_dvs_append(&$1, $3); $$ = $1; }
;

dataval: dvexpr		{ $$ = yasm_dv_new_expr($1); }
    | STRING		{ $$ = yasm_dv_new_string($1); }
    | error		{
	cur_we->error(cur_lindex, N_("expression syntax error"));
	$$ = (yasm_dataval *)NULL;
    }
;

label: label_id	    {
	yasm_symrec_define_label($1, nasm_parser_cur_section,
				 nasm_parser_prev_bc, 1, cur_lindex);
	xfree($1);
    }
    | label_id ':'  {
	yasm_symrec_define_label($1, nasm_parser_cur_section,
				 nasm_parser_prev_bc, 1, cur_lindex);
	xfree($1);
    }
;

label_id: ID	    {
	$$ = $1;
	if (nasm_parser_locallabel_base)
	    xfree(nasm_parser_locallabel_base);
	nasm_parser_locallabel_base_len = strlen($1);
	nasm_parser_locallabel_base =
	    xmalloc(nasm_parser_locallabel_base_len+1);
	strcpy(nasm_parser_locallabel_base, $1);
    }
    | SPECIAL_ID
    | LOCAL_ID
;

label_id_equ: ID
    | SPECIAL_ID
    | LOCAL_ID
;

/* directives */
directive: DIRECTIVE_NAME directive_val	{
	xfree($1);
    }
    | DIRECTIVE_NAME error		{
	cur_we->error(cur_lindex, N_("invalid arguments to [%s]"), $1);
	xfree($1);
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
	yasm_vps_initialize(&$$);
	yasm_vps_append(&$$, $1);
    }
    | directive_valparams directive_valparam	{
	yasm_vps_append(&$1, $2);
	$$ = $1;
    }
    | directive_valparams ',' directive_valparam    {
	yasm_vps_append(&$1, $3);
	$$ = $1;
    }
;

directive_valparam: direxpr	{
	/* If direxpr is just an ID, put it in val and delete the expr.
	 * Otherwise, we need to go through the expr and replace the current
	 * (local) symrecs with the use of global ones.
	 */
	const /*@null@*/ yasm_symrec *vp_symrec;
	if ((vp_symrec = yasm_expr_get_symrec(&$1, 0))) {
	    yasm_vp_new($$, xstrdup(yasm_symrec_get_name(vp_symrec)), NULL);
	    yasm_expr_delete($1);
	} else {
	    yasm_expr__traverse_leaves_in($1, NULL, fix_directive_symrec);
	    yasm_vp_new($$, NULL, $1);
	}
    }
    | STRING			{ yasm_vp_new($$, $1, NULL); }
    | ID '=' direxpr		{
	yasm_expr__traverse_leaves_in($3, NULL, fix_directive_symrec);
	yasm_vp_new($$, $1, $3);
    }
;

/* memory addresses */
memaddr: expr		    {
	$$ = nasm_parser_arch->parse.ea_new_expr($1);
    }
    | SEGREG ':' memaddr    {
	$$ = $3;
	nasm_parser_arch->parse.handle_seg_override($$, $1[0], cur_lindex);
    }
    | BYTE memaddr	    { $$ = $2; yasm_ea_set_len($$, 1); }
    | WORD memaddr	    { $$ = $2; yasm_ea_set_len($$, 2); }
    | DWORD memaddr	    { $$ = $2; yasm_ea_set_len($$, 4); }
    | QWORD memaddr	    { $$ = $2; yasm_ea_set_len($$, 8); }
    | NOSPLIT memaddr	    { $$ = $2; yasm_ea_set_nosplit($$, 1); }
;

/* instruction operands */
operands: operand	    {
	yasm_ops_initialize(&$$.operands);
	yasm_ops_append(&$$.operands, $1);
	$$.num_operands = 1;
    }
    | operands ',' operand  {
	yasm_ops_append(&$1.operands, $3);
	$$.operands = $1.operands;
	$$.num_operands = $1.num_operands+1;
    }
;

operand: '[' memaddr ']'    { $$ = yasm_operand_new_mem($2); }
    | expr		    { $$ = yasm_operand_new_imm($1); }
    | SEGREG		    { $$ = yasm_operand_new_segreg($1[0]); }
    | BYTE operand	    {
	$$ = $2;
	if ($$->type == YASM_INSN__OPERAND_REG &&
	    nasm_parser_arch->get_reg_size($$->data.reg) != 1)
	    cur_we->error(cur_lindex, N_("cannot override register size"));
	else
	    $$->size = 1;
    }
    | WORD operand	    {
	$$ = $2;
	if ($$->type == YASM_INSN__OPERAND_REG &&
	    nasm_parser_arch->get_reg_size($$->data.reg) != 2)
	    cur_we->error(cur_lindex, N_("cannot override register size"));
	else
	    $$->size = 2;
    }
    | DWORD operand	    {
	$$ = $2;
	if ($$->type == YASM_INSN__OPERAND_REG &&
	    nasm_parser_arch->get_reg_size($$->data.reg) != 4)
	    cur_we->error(cur_lindex, N_("cannot override register size"));
	else
	    $$->size = 4;
    }
    | QWORD operand	    {
	$$ = $2;
	if ($$->type == YASM_INSN__OPERAND_REG &&
	    nasm_parser_arch->get_reg_size($$->data.reg) != 8)
	    cur_we->error(cur_lindex, N_("cannot override register size"));
	else
	    $$->size = 8;
    }
    | TWORD operand	    {
	$$ = $2;
	if ($$->type == YASM_INSN__OPERAND_REG &&
	    nasm_parser_arch->get_reg_size($$->data.reg) != 10)
	    cur_we->error(cur_lindex, N_("cannot override register size"));
	else
	    $$->size = 10;
    }
    | DQWORD operand	    {
	$$ = $2;
	if ($$->type == YASM_INSN__OPERAND_REG &&
	    nasm_parser_arch->get_reg_size($$->data.reg) != 16)
	    cur_we->error(cur_lindex, N_("cannot override register size"));
	else
	    $$->size = 16;
    }
    | TARGETMOD operand	    { $$ = $2; $$->targetmod = $1[0]; }
;

/* expression trees */

/* expr w/o FLTNUM and unary + and -, for use in directives */
direxpr: INTNUM			{ $$ = p_expr_new_ident(yasm_expr_int($1)); }
    | ID			{
	$$ = p_expr_new_ident(yasm_expr_sym(
	    yasm_symrec_define_label($1, NULL, NULL, 0, cur_lindex)));
	xfree($1);
    }
    | direxpr '|' direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_OR, $3); }
    | direxpr '^' direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_XOR, $3); }
    | direxpr '&' direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_AND, $3); }
    | direxpr LEFT_OP direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SHL, $3); }
    | direxpr RIGHT_OP direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SHR, $3); }
    | direxpr '+' direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_ADD, $3); }
    | direxpr '-' direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SUB, $3); }
    | direxpr '*' direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_MUL, $3); }
    | direxpr '/' direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_DIV, $3); }
    | direxpr SIGNDIV direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SIGNDIV, $3); }
    | direxpr '%' direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_MOD, $3); }
    | direxpr SIGNMOD direxpr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SIGNMOD, $3); }
    /*| '!' expr	    { $$ = p_expr_new_branch(YASM_EXPR_LNOT, $2); }*/
    | '~' direxpr %prec UNARYOP	{ $$ = p_expr_new_branch(YASM_EXPR_NOT, $2); }
    | '(' direxpr ')'		{ $$ = $2; }
;

dvexpr: INTNUM		    { $$ = p_expr_new_ident(yasm_expr_int($1)); }
    | FLTNUM		    { $$ = p_expr_new_ident(yasm_expr_float($1)); }
    | explabel		    { $$ = p_expr_new_ident(yasm_expr_sym($1)); }
    /*| dvexpr '||' dvexpr  { $$ = p_expr_new_tree($1, YASM_EXPR_LOR, $3); }*/
    | dvexpr '|' dvexpr	    { $$ = p_expr_new_tree($1, YASM_EXPR_OR, $3); }
    | dvexpr '^' dvexpr	    { $$ = p_expr_new_tree($1, YASM_EXPR_XOR, $3); }
    /*| dvexpr '&&' dvexpr  { $$ = p_expr_new_tree($1, YASM_EXPR_LAND, $3); }*/
    | dvexpr '&' dvexpr	    { $$ = p_expr_new_tree($1, YASM_EXPR_AND, $3); }
    /*| dvexpr '==' dvexpr  { $$ = p_expr_new_tree($1, YASM_EXPR_EQUALS, $3); }*/
    /*| dvexpr '>' dvexpr   { $$ = p_expr_new_tree($1, YASM_EXPR_GT, $3); }*/
    /*| dvexpr '<' dvexpr   { $$ = p_expr_new_tree($1, YASM_EXPR_GT, $3); }*/
    /*| dvexpr '>=' dvexpr  { $$ = p_expr_new_tree($1, YASM_EXPR_GE, $3); }*/
    /*| dvexpr '<=' dvexpr  { $$ = p_expr_new_tree($1, YASM_EXPR_GE, $3); }*/
    /*| dvexpr '!=' dvexpr  { $$ = p_expr_new_tree($1, YASM_EXPR_NE, $3); }*/
    | dvexpr LEFT_OP dvexpr { $$ = p_expr_new_tree($1, YASM_EXPR_SHL, $3); }
    | dvexpr RIGHT_OP dvexpr { $$ = p_expr_new_tree($1, YASM_EXPR_SHR, $3); }
    | dvexpr '+' dvexpr	    { $$ = p_expr_new_tree($1, YASM_EXPR_ADD, $3); }
    | dvexpr '-' dvexpr	    { $$ = p_expr_new_tree($1, YASM_EXPR_SUB, $3); }
    | dvexpr '*' dvexpr	    { $$ = p_expr_new_tree($1, YASM_EXPR_MUL, $3); }
    | dvexpr '/' dvexpr	    { $$ = p_expr_new_tree($1, YASM_EXPR_DIV, $3); }
    | dvexpr SIGNDIV dvexpr { $$ = p_expr_new_tree($1, YASM_EXPR_SIGNDIV, $3); }
    | dvexpr '%' dvexpr	    { $$ = p_expr_new_tree($1, YASM_EXPR_MOD, $3); }
    | dvexpr SIGNMOD dvexpr { $$ = p_expr_new_tree($1, YASM_EXPR_SIGNMOD, $3); }
    | '+' dvexpr %prec UNARYOP  { $$ = $2; }
    | '-' dvexpr %prec UNARYOP  { $$ = p_expr_new_branch(YASM_EXPR_NEG, $2); }
    /*| '!' dvexpr	    { $$ = p_expr_new_branch(YASM_EXPR_LNOT, $2); }*/
    | '~' dvexpr %prec UNARYOP  { $$ = p_expr_new_branch(YASM_EXPR_NOT, $2); }
    | SEG dvexpr		{ $$ = p_expr_new_branch(YASM_EXPR_SEG, $2); }
    | WRT dvexpr		{ $$ = p_expr_new_branch(YASM_EXPR_WRT, $2); }
    | '(' dvexpr ')'		{ $$ = $2; }
;

/* Expressions for operands and memory expressions.
 * We don't attempt to check memory expressions for validity here.
 * Essentially the same as expr_no_string above but adds REG and STRING.
 */
expr: INTNUM		{ $$ = p_expr_new_ident(yasm_expr_int($1)); }
    | FLTNUM		{ $$ = p_expr_new_ident(yasm_expr_float($1)); }
    | REG		{ $$ = p_expr_new_ident(yasm_expr_reg($1[0])); }
    | STRING		{
	$$ = p_expr_new_ident(yasm_expr_int(
	    yasm_intnum_new_charconst_nasm($1, cur_lindex)));
	xfree($1);
    }
    | explabel		{ $$ = p_expr_new_ident(yasm_expr_sym($1)); }
    /*| expr '||' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_LOR, $3); }*/
    | expr '|' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_OR, $3); }
    | expr '^' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_XOR, $3); }
    /*| expr '&&' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_LAND, $3); }*/
    | expr '&' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_AND, $3); }
    /*| expr '==' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_EQUALS, $3); }*/
    /*| expr '>' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_GT, $3); }*/
    /*| expr '<' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_GT, $3); }*/
    /*| expr '>=' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_GE, $3); }*/
    /*| expr '<=' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_GE, $3); }*/
    /*| expr '!=' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_NE, $3); }*/
    | expr LEFT_OP expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SHL, $3); }
    | expr RIGHT_OP expr { $$ = p_expr_new_tree($1, YASM_EXPR_SHR, $3); }
    | expr '+' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_ADD, $3); }
    | expr '-' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SUB, $3); }
    | expr '*' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_MUL, $3); }
    | expr '/' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_DIV, $3); }
    | expr SIGNDIV expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SIGNDIV, $3); }
    | expr '%' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_MOD, $3); }
    | expr SIGNMOD expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SIGNMOD, $3); }
    | '+' expr %prec UNARYOP	{ $$ = $2; }
    | '-' expr %prec UNARYOP	{ $$ = p_expr_new_branch(YASM_EXPR_NEG, $2); }
    /*| '!' expr	{ $$ = p_expr_new_branch(YASM_EXPR_LNOT, $2); }*/
    | '~' expr %prec UNARYOP	{ $$ = p_expr_new_branch(YASM_EXPR_NOT, $2); }
    | SEG expr		{ $$ = p_expr_new_branch(YASM_EXPR_SEG, $2); }
    | WRT expr		{ $$ = p_expr_new_branch(YASM_EXPR_WRT, $2); }
    | expr ':' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SEGOFF, $3); }
    | '(' expr ')'	{ $$ = $2; }
;

explabel: ID		{
	$$ = yasm_symrec_use($1, cur_lindex);
	xfree($1);
    }
    | SPECIAL_ID	{
	$$ = yasm_symrec_use($1, cur_lindex);
	xfree($1);
    }
    | LOCAL_ID		{
	$$ = yasm_symrec_use($1, cur_lindex);
	xfree($1);
    }
    | '$'		{
	/* "$" references the current assembly position */
	$$ = yasm_symrec_define_label("$", nasm_parser_cur_section,
				      nasm_parser_prev_bc, 0, cur_lindex);
    }
    | START_SECTION_ID	{
	/* "$$" references the start of the current section */
	$$ = yasm_symrec_define_label("$$", nasm_parser_cur_section, NULL, 0,
				      cur_lindex);
    }
;

%%
/*@=usedef =nullassign =memtrans =usereleased =compdef =mustfree@*/

static int
fix_directive_symrec(yasm_expr__item *ei, /*@unused@*/ void *d)
{
    if (!ei || ei->type != YASM_EXPR_SYM)
	return 0;

    /* FIXME: Delete current symrec */
    ei->data.sym =
	yasm_symrec_use(yasm_symrec_get_name(ei->data.sym), cur_lindex);

    return 0;
}

static void
nasm_parser_directive(const char *name, yasm_valparamhead *valparams,
		      yasm_valparamhead *objext_valparams)
{
    yasm_valparam *vp, *vp2;
    yasm_symrec *sym;
    unsigned long lindex = cur_lindex;

    /* Handle (mostly) output-format independent directives here */
    if (strcasecmp(name, "extern") == 0) {
	vp = yasm_vps_first(valparams);
	if (vp->val) {
	    sym = yasm_symrec_declare(vp->val, YASM_SYM_EXTERN, lindex);
	    if (nasm_parser_objfmt->extern_declare)
		nasm_parser_objfmt->extern_declare(sym, objext_valparams,
						   lindex);
	} else
	    cur_we->error(lindex, N_("invalid argument to [%s]"), "EXTERN");
    } else if (strcasecmp(name, "global") == 0) {
	vp = yasm_vps_first(valparams);
	if (vp->val) {
	    sym = yasm_symrec_declare(vp->val, YASM_SYM_GLOBAL, lindex);
	    if (nasm_parser_objfmt->global_declare)
		nasm_parser_objfmt->global_declare(sym, objext_valparams,
						   lindex);
	} else
	    cur_we->error(lindex, N_("invalid argument to [%s]"), "GLOBAL");
    } else if (strcasecmp(name, "common") == 0) {
	vp = yasm_vps_first(valparams);
	if (vp->val) {
	    vp2 = yasm_vps_next(vp);
	    if (!vp2 || (!vp2->val && !vp2->param))
		cur_we->error(lindex,
			      N_("no size specified in %s declaration"),
			      "COMMON");
	    else {
		if (vp2->val) {
		    sym = yasm_symrec_declare(vp->val, YASM_SYM_COMMON,
					      lindex);
		    if (nasm_parser_objfmt->common_declare)
			nasm_parser_objfmt->common_declare(sym,
			    p_expr_new_ident(yasm_expr_sym(
				yasm_symrec_use(vp2->val, lindex))),
			    objext_valparams, lindex);
		} else if (vp2->param) {
		    sym = yasm_symrec_declare(vp->val, YASM_SYM_COMMON,
					      lindex);
		    if (nasm_parser_objfmt->common_declare)
			nasm_parser_objfmt->common_declare(sym, vp2->param,
							   objext_valparams,
							   lindex);
		    vp2->param = NULL;
		}
	    }
	} else
	    cur_we->error(lindex, N_("invalid argument to [%s]"), "COMMON");
    } else if (strcasecmp(name, "section") == 0 ||
	       strcasecmp(name, "segment") == 0) {
	yasm_section *new_section =
	    nasm_parser_objfmt->sections_switch(&nasm_parser_sections,
						valparams, objext_valparams,
						lindex);
	if (new_section) {
	    nasm_parser_cur_section = new_section;
	    nasm_parser_prev_bc =
		yasm_bcs_last(yasm_section_get_bytecodes(new_section));
	} else
	    cur_we->error(lindex, N_("invalid argument to [%s]"), "SECTION");
    } else if (strcasecmp(name, "absolute") == 0) {
	/* it can be just an ID or a complete expression, so handle both. */
	vp = yasm_vps_first(valparams);
	if (vp->val)
	    nasm_parser_cur_section =
		yasm_sections_switch_absolute(&nasm_parser_sections,
		    p_expr_new_ident(yasm_expr_sym(
			yasm_symrec_use(vp->val, lindex))),
		    cur_we->internal_error_);
	else if (vp->param) {
	    nasm_parser_cur_section =
		yasm_sections_switch_absolute(&nasm_parser_sections, vp->param,
					      cur_we->internal_error_);
	    vp->param = NULL;
	}
	nasm_parser_prev_bc = (yasm_bytecode *)NULL;
    } else if (strcasecmp(name, "cpu") == 0) {
	yasm_vps_foreach(vp, valparams) {
	    if (vp->val)
		nasm_parser_arch->parse.switch_cpu(vp->val, lindex);
	    else if (vp->param) {
		const yasm_intnum *intcpu;
		intcpu = yasm_expr_get_intnum(&vp->param, NULL);
		if (!intcpu)
		    cur_we->error(lindex, N_("invalid argument to [%s]"),
				  "CPU");
		else {
		    char strcpu[16];
		    sprintf(strcpu, "%lu", yasm_intnum_get_uint(intcpu));
		    nasm_parser_arch->parse.switch_cpu(strcpu, lindex);
		}
	    }
	}
    } else if (!nasm_parser_arch->parse.directive(name, valparams,
						  objext_valparams,
						  &nasm_parser_sections,
						  lindex)) {
	;
    } else if (nasm_parser_objfmt->directive(name, valparams, objext_valparams,
					     &nasm_parser_sections, lindex)) {
	cur_we->error(lindex, N_("unrecognized directive [%s]"), name);
    }

    yasm_vps_delete(valparams);
    if (objext_valparams)
	yasm_vps_delete(objext_valparams);
}

static void
nasm_parser_error(const char *s)
{
    cur_we->parser_error(cur_lindex, s);
}

