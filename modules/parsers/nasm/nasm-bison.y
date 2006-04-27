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
#include <util.h>
RCSID("$Id$");

#define YASM_LIB_INTERNAL
#define YASM_EXPR_INTERNAL
#include <libyasm.h>

#ifdef STDC_HEADERS
# include <math.h>
#endif

#include "modules/parsers/nasm/nasm-parser.h"
#include "modules/parsers/nasm/nasm-defs.h"

static void nasm_parser_directive
    (yasm_parser_nasm *parser_nasm, const char *name,
     yasm_valparamhead *valparams,
     /*@null@*/ yasm_valparamhead *objext_valparams);
static int fix_directive_symrec(/*@null@*/ yasm_expr__item *ei,
				/*@null@*/ void *d);
static void define_label(yasm_parser_nasm *parser_nasm, /*@only@*/ char *name,
			 int local);

#define nasm_parser_error(s)	yasm__parser_error(cur_line, s)
#define YYPARSE_PARAM	parser_nasm_arg
#define YYLEX_PARAM	parser_nasm_arg
#define parser_nasm	((yasm_parser_nasm *)parser_nasm_arg)
#define nasm_parser_debug   (parser_nasm->debug)

/*@-usedef -nullassign -memtrans -usereleased -compdef -mustfree@*/
%}

%pure_parser

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
	yasm_insn_operands operands;
	int num_operands;
    } insn_operands;
    yasm_insn_operand *insn_operand;
    struct {
	char *name;
	int local;
    } label;
    struct {
	char *contents;
	size_t len;
    } str;
}

%token <intn> INTNUM
%token <flt> FLTNUM
%token <str_val> DIRECTIVE_NAME FILENAME
%token <str> STRING
%token <int_info> SIZE_OVERRIDE
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
%type <label> label_id label
%type <data> dataval
%type <datahead> datavals
%type <dir_valparams> directive_valparams
%type <dir_valparam> directive_valparam
%type <insn_operands> operands
%type <insn_operand> operand

%left ':'
%left WRT
%left '|'
%left '^'
%left '&'
%left LEFT_OP RIGHT_OP
%left '-' '+'
%left '*' '/' SIGNDIV '%' SIGNMOD
%nonassoc UNARYOP
%nonassoc SEG

%%
input: /* empty */
    | input line    {
	parser_nasm->temp_bc =
	    yasm_section_bcs_append(parser_nasm->cur_section, $2);
	if (parser_nasm->temp_bc)
	    parser_nasm->prev_bc = parser_nasm->temp_bc;
	if (parser_nasm->save_input)
	    yasm_linemap_add_source(parser_nasm->linemap,
		parser_nasm->temp_bc,
		(char *)parser_nasm->save_line[parser_nasm->save_last ^ 1]);
	yasm_linemap_goto_next(parser_nasm->linemap);
    }
;

line: '\n'		{ $$ = (yasm_bytecode *)NULL; }
    | lineexp '\n'
    | LINE INTNUM '+' INTNUM FILENAME '\n' {
	/* %line indicates the line number of the *next* line, so subtract out
	 * the increment when setting the line number.
	 */
	yasm_linemap_set(parser_nasm->linemap, $5,
	    yasm_intnum_get_uint($2) - yasm_intnum_get_uint($4),
	    yasm_intnum_get_uint($4));
	yasm_intnum_destroy($2);
	yasm_intnum_destroy($4);
	yasm_xfree($5);
	$$ = (yasm_bytecode *)NULL;
    }
    | '[' { parser_nasm->state = DIRECTIVE; } directive ']' '\n' {
	$$ = (yasm_bytecode *)NULL;
    }
    | error '\n'	{
	yasm__error(cur_line,
		    N_("label or instruction expected at start of line"));
	$$ = (yasm_bytecode *)NULL;
	yyerrok;
    }
;

lineexp: exp
    | TIMES expr exp		{ $$ = $3; yasm_bc_set_multiple($$, $2); }
    | label_id			{
	yasm__warning(YASM_WARN_ORPHAN_LABEL, cur_line,
		      N_("label alone on a line without a colon might be in error"));
	$$ = (yasm_bytecode *)NULL;
	define_label(parser_nasm, $1.name, $1.local);
    }
    | label_id ':'		{
	$$ = (yasm_bytecode *)NULL;
	define_label(parser_nasm, $1.name, $1.local);
    }
    | label exp			{
	$$ = $2;
	define_label(parser_nasm, $1.name, $1.local);
    }
    | label TIMES expr exp	{
	$$ = $4;
	yasm_bc_set_multiple($$, $3);
	define_label(parser_nasm, $1.name, $1.local);
    }
    | label EQU expr		{
	$$ = (yasm_bytecode *)NULL;
	yasm_symtab_define_equ(p_symtab, $1.name, $3, cur_line);
	yasm_xfree($1.name);
    }
;

exp: instr
    | DECLARE_DATA datavals		{
	$$ = yasm_bc_create_data(&$2, $1, 0, cur_line);
    }
    | RESERVE_SPACE expr		{
	$$ = yasm_bc_create_reserve($2, $1, cur_line);
    }
    | INCBIN STRING			{
	$$ = yasm_bc_create_incbin($2.contents, NULL, NULL, cur_line);
    }
    | INCBIN STRING ','			{
	$$ = yasm_bc_create_incbin($2.contents, NULL, NULL, cur_line);
    }
    | INCBIN STRING ',' expr		{
	$$ = yasm_bc_create_incbin($2.contents, $4, NULL, cur_line);
    }
    | INCBIN STRING ',' expr ','	{
	$$ = yasm_bc_create_incbin($2.contents, $4, NULL, cur_line);
    }
    | INCBIN STRING ',' expr ',' expr	{
	$$ = yasm_bc_create_incbin($2.contents, $4, $6, cur_line);
    }
;

instr: INSN		{
	$$ = yasm_bc_create_insn(parser_nasm->arch, $1, 0, NULL, cur_line);
    }
    | INSN operands	{
	$$ = yasm_bc_create_insn(parser_nasm->arch, $1, $2.num_operands,
				 &$2.operands, cur_line);
    }
    | INSN error	{
	yasm__error(cur_line, N_("expression syntax error"));
	$$ = NULL;
    }
    | PREFIX instr	{
	$$ = $2;
	yasm_bc_insn_add_prefix($$, $1);
    }
    | SEGREG instr	{
	$$ = $2;
	yasm_bc_insn_add_seg_prefix($$, $1[0]);
    }
;

datavals: dataval	    {
	yasm_dvs_initialize(&$$);
	yasm_dvs_append(&$$, $1);
    }
    | datavals ',' dataval  { yasm_dvs_append(&$1, $3); $$ = $1; }
    | datavals ','	    { $$ = $1; }
;

dataval: dvexpr		{ $$ = yasm_dv_create_expr($1); }
    | STRING		{
	$$ = yasm_dv_create_string($1.contents, $1.len);
    }
    | error		{
	yasm__error(cur_line, N_("expression syntax error"));
	$$ = (yasm_dataval *)NULL;
    }
;

label: label_id
    | label_id ':'	{ $$ = $1; }
;

label_id: ID		{ $$.name = $1; $$.local = 0; }
    | SPECIAL_ID	{ $$.name = $1; $$.local = 1; }
    | LOCAL_ID		{ $$.name = $1; $$.local = 1; }
;

/* directives */
directive: DIRECTIVE_NAME directive_val	{
	yasm_xfree($1);
    }
    | DIRECTIVE_NAME error		{
	yasm__error(cur_line, N_("invalid arguments to [%s]"), $1);
	yasm_xfree($1);
    }
;

    /* $<str_val>0 is the DIRECTIVE_NAME */
    /* After : is (optional) object-format specific extension */
directive_val: directive_valparams {
	nasm_parser_directive(parser_nasm, $<str_val>0, &$1, NULL);
    }
    | directive_valparams ':' directive_valparams {
	nasm_parser_directive(parser_nasm, $<str_val>0, &$1, &$3);
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
	    $$ = yasm_vp_create(yasm__xstrdup(yasm_symrec_get_name(vp_symrec)),
				NULL);
	    yasm_expr_destroy($1);
	} else {
	    yasm_expr__traverse_leaves_in($1, parser_nasm,
					  fix_directive_symrec);
	    $$ = yasm_vp_create(NULL, $1);
	}
    }
    | STRING			{ $$ = yasm_vp_create($1.contents, NULL); }
    | ID '=' direxpr		{
	yasm_expr__traverse_leaves_in($3, parser_nasm, fix_directive_symrec);
	$$ = yasm_vp_create($1, $3);
    }
;

/* memory addresses */
memaddr: expr		    {
	$$ = yasm_arch_ea_create(parser_nasm->arch, $1);
    }
    | SEGREG ':' memaddr    {
	$$ = $3;
	yasm_ea_set_segreg($$, $1[0], cur_line);
    }
    | SIZE_OVERRIDE memaddr { $$ = $2; yasm_ea_set_len($$, $1); }
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

operand: '[' memaddr ']'    { $$ = yasm_operand_create_mem($2); }
    | expr		    { $$ = yasm_operand_create_imm($1); }
    | SEGREG		    { $$ = yasm_operand_create_segreg($1[0]); }
    | SIZE_OVERRIDE operand {
	$$ = $2;
	if ($$->type == YASM_INSN__OPERAND_REG &&
	    yasm_arch_get_reg_size(parser_nasm->arch, $$->data.reg) != $1)
	    yasm__error(cur_line, N_("cannot override register size"));
	else
	    $$->size = $1;
    }
    | TARGETMOD operand	    { $$ = $2; $$->targetmod = $1[0]; }
;

/* expression trees */

/* expr w/o FLTNUM and unary + and -, for use in directives */
direxpr: INTNUM			{ $$ = p_expr_new_ident(yasm_expr_int($1)); }
    | ID			{
	$$ = p_expr_new_ident(yasm_expr_sym(
	    yasm_symtab_define_label(p_symtab, $1,
		yasm_section_bcs_first(parser_nasm->cur_section), 0,
		cur_line)));
	yasm_xfree($1);
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
    | dvexpr WRT dvexpr	    { $$ = p_expr_new_tree($1, YASM_EXPR_WRT, $3); }
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
	    yasm_intnum_create_charconst_nasm($1.contents, cur_line)));
	yasm_xfree($1.contents);
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
    | expr WRT expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_WRT, $3); }
    | expr ':' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SEGOFF, $3); }
    | '(' expr ')'	{ $$ = $2; }
;

explabel: ID		{
	$$ = yasm_symtab_use(p_symtab, $1, cur_line);
	yasm_xfree($1);
    }
    | SPECIAL_ID	{
	$$ = yasm_symtab_use(p_symtab, $1, cur_line);
	yasm_xfree($1);
    }
    | LOCAL_ID		{
	$$ = yasm_symtab_use(p_symtab, $1, cur_line);
	yasm_xfree($1);
    }
    | '$'		{
	/* "$" references the current assembly position */
	$$ = yasm_symtab_define_curpos(p_symtab, "$", parser_nasm->prev_bc,
				       cur_line);
    }
    | START_SECTION_ID	{
	/* "$$" references the start of the current section */
	$$ = yasm_symtab_define_label(p_symtab, "$$",
	    yasm_section_bcs_first(parser_nasm->cur_section), 0, cur_line);
    }
;

%%
/*@=usedef =nullassign =memtrans =usereleased =compdef =mustfree@*/

#undef parser_nasm

static void
define_label(yasm_parser_nasm *parser_nasm, char *name, int local)
{
    if (!local) {
	if (parser_nasm->locallabel_base)
	    yasm_xfree(parser_nasm->locallabel_base);
	parser_nasm->locallabel_base_len = strlen(name);
	parser_nasm->locallabel_base =
	    yasm_xmalloc(parser_nasm->locallabel_base_len+1);
	strcpy(parser_nasm->locallabel_base, name);
    }

    yasm_symtab_define_label(p_symtab, name, parser_nasm->prev_bc, 1,
			     cur_line);
    yasm_xfree(name);
}

static int
fix_directive_symrec(yasm_expr__item *ei, void *d)
{
    yasm_parser_nasm *parser_nasm = (yasm_parser_nasm *)d;
    if (!ei || ei->type != YASM_EXPR_SYM)
	return 0;

    /* FIXME: Delete current symrec */
    ei->data.sym =
	yasm_symtab_use(p_symtab, yasm_symrec_get_name(ei->data.sym),
			cur_line);

    return 0;
}

static void
nasm_parser_directive(yasm_parser_nasm *parser_nasm, const char *name,
		      yasm_valparamhead *valparams,
		      yasm_valparamhead *objext_valparams)
{
    yasm_valparam *vp, *vp2;
    unsigned long line = cur_line;

    /* Handle (mostly) output-format independent directives here */
    if (yasm__strcasecmp(name, "extern") == 0) {
	vp = yasm_vps_first(valparams);
	if (vp->val) {
	    yasm_objfmt_extern_declare(parser_nasm->objfmt, vp->val,
				       objext_valparams, line);
	} else
	    yasm__error(line, N_("invalid argument to [%s]"), "EXTERN");
    } else if (yasm__strcasecmp(name, "global") == 0) {
	vp = yasm_vps_first(valparams);
	if (vp->val) {
	    yasm_objfmt_global_declare(parser_nasm->objfmt, vp->val,
				       objext_valparams, line);
	} else
	    yasm__error(line, N_("invalid argument to [%s]"), "GLOBAL");
    } else if (yasm__strcasecmp(name, "common") == 0) {
	vp = yasm_vps_first(valparams);
	if (vp->val) {
	    vp2 = yasm_vps_next(vp);
	    if (!vp2 || (!vp2->val && !vp2->param))
		yasm__error(line, N_("no size specified in %s declaration"),
			    "COMMON");
	    else {
		if (vp2->val) {
		    yasm_objfmt_common_declare(parser_nasm->objfmt, vp->val,
			p_expr_new_ident(yasm_expr_sym(
			    yasm_symtab_use(p_symtab, vp2->val, line))),
			objext_valparams, line);
		} else if (vp2->param) {
		    yasm_objfmt_common_declare(parser_nasm->objfmt, vp->val,
					       vp2->param, objext_valparams,
					       line);
		    vp2->param = NULL;
		}
	    }
	} else
	    yasm__error(line, N_("invalid argument to [%s]"), "COMMON");
    } else if (yasm__strcasecmp(name, "section") == 0 ||
	       yasm__strcasecmp(name, "segment") == 0) {
	yasm_section *new_section =
	    yasm_objfmt_section_switch(parser_nasm->objfmt, valparams,
				       objext_valparams, line);
	if (new_section) {
	    parser_nasm->cur_section = new_section;
	    parser_nasm->prev_bc = yasm_section_bcs_last(new_section);
	} else
	    yasm__error(line, N_("invalid argument to [%s]"), "SECTION");
    } else if (yasm__strcasecmp(name, "absolute") == 0) {
	/* it can be just an ID or a complete expression, so handle both. */
	vp = yasm_vps_first(valparams);
	if (vp->val)
	    parser_nasm->cur_section =
		yasm_object_create_absolute(parser_nasm->object,
		    p_expr_new_ident(yasm_expr_sym(
			yasm_symtab_use(p_symtab, vp->val, line))), line);
	else if (vp->param) {
	    parser_nasm->cur_section =
		yasm_object_create_absolute(parser_nasm->object, vp->param,
					    line);
	    vp->param = NULL;
	}
	parser_nasm->prev_bc = yasm_section_bcs_last(parser_nasm->cur_section);
    } else if (yasm__strcasecmp(name, "align") == 0) {
	/*@only@*/ yasm_expr *boundval;
	/*@depedent@*/ yasm_intnum *boundintn;

	/* it can be just an ID or a complete expression, so handle both. */
	vp = yasm_vps_first(valparams);
	if (vp->val)
	    boundval = p_expr_new_ident(yasm_expr_sym(
		yasm_symtab_use(p_symtab, vp->val, line)));
	else if (vp->param) {
	    boundval = vp->param;
	    vp->param = NULL;
	}

	/* Largest .align in the section specifies section alignment.
	 * Note: this doesn't match NASM behavior, but is a lot more
	 * intelligent!
	 */
	boundintn = yasm_expr_get_intnum(&boundval, NULL);
	if (boundintn) {
	    unsigned long boundint = yasm_intnum_get_uint(boundintn);

	    /* Alignments must be a power of two. */
	    if ((boundint & (boundint - 1)) == 0) {
		if (boundint > yasm_section_get_align(parser_nasm->cur_section))
		    yasm_section_set_align(parser_nasm->cur_section, boundint,
					   cur_line);
	    }
	}

	/* As this directive is called only when nop is used as fill, always
	 * use arch (nop) fill.
	 */
	parser_nasm->prev_bc =
	    yasm_section_bcs_append(parser_nasm->cur_section,
		yasm_bc_create_align(boundval, NULL, NULL,
		    /*yasm_section_is_code(parser_nasm->cur_section) ?*/
			yasm_arch_get_fill(parser_nasm->arch)/* : NULL*/,
		    cur_line));
    } else if (yasm__strcasecmp(name, "cpu") == 0) {
	yasm_vps_foreach(vp, valparams) {
	    if (vp->val)
		yasm_arch_parse_cpu(parser_nasm->arch, vp->val,
				    strlen(vp->val), line);
	    else if (vp->param) {
		const yasm_intnum *intcpu;
		intcpu = yasm_expr_get_intnum(&vp->param, NULL);
		if (!intcpu)
		    yasm__error(line, N_("invalid argument to [%s]"), "CPU");
		else {
		    char strcpu[16];
		    sprintf(strcpu, "%lu", yasm_intnum_get_uint(intcpu));
		    yasm_arch_parse_cpu(parser_nasm->arch, strcpu,
					strlen(strcpu), line);
		}
	    }
	}
    } else if (!yasm_arch_parse_directive(parser_nasm->arch, name, valparams,
		    objext_valparams, parser_nasm->object, line)) {
	;
    } else if (!yasm_dbgfmt_directive(parser_nasm->dbgfmt, name,
				      parser_nasm->cur_section, valparams,
				      line)) {
	;
    } else if (yasm_objfmt_directive(parser_nasm->objfmt, name, valparams,
				     objext_valparams, line)) {
	yasm__error(line, N_("unrecognized directive [%s]"), name);
    }

    yasm_vps_delete(valparams);
    if (objext_valparams)
	yasm_vps_delete(objext_valparams);
}
