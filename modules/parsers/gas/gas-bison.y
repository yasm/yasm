/*
 * GAS-compatible bison parser
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
%{
#include <util.h>
RCSID("$Id$");

#define YASM_LIB_INTERNAL
#define YASM_EXPR_INTERNAL
#include <libyasm.h>

#ifdef STDC_HEADERS
# include <math.h>
#endif

#include "modules/parsers/gas/gas-parser.h"
#include "modules/parsers/gas/gas-defs.h"

static void define_label(yasm_parser_gas *parser_gas, char *name, int local);
static yasm_section *gas_get_section(yasm_parser_gas *parser_gas, char *name,
				     /*@null@*/ char *flags,
				     /*@null@*/ char *type);
static void gas_switch_section(yasm_parser_gas *parser_gas, char *name,
			       /*@null@*/ char *flags, /*@null@*/ char *type);
static yasm_bytecode *gas_define_strings(yasm_parser_gas *parser_gas,
					 yasm_valparamhead *vps, int withzero);
static yasm_bytecode *gas_define_data(yasm_parser_gas *parser_gas,
				      yasm_valparamhead *vps,
				      unsigned int size);
static void gas_parser_directive
    (yasm_parser_gas *parser_gas, const char *name,
     yasm_valparamhead *valparams,
     /*@null@*/ yasm_valparamhead *objext_valparams);

#define gas_parser_error(s)	yasm__parser_error(cur_line, s)
#define YYPARSE_PARAM	parser_gas_arg
#define YYLEX_PARAM	parser_gas_arg
#define parser_gas	((yasm_parser_gas *)parser_gas_arg)
#define gas_parser_debug   (parser_gas->debug)

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
    yasm_bytecode *bc;
    yasm_valparamhead valparams;
    struct {
	yasm_insn_operands operands;
	int num_operands;
    } insn_operands;
    yasm_insn_operand *insn_operand;
}

%token <intn> INTNUM
%token <flt> FLTNUM
%token <str_val> STRING
%token <int_info> SIZE_OVERRIDE
%token <int_info> DECLARE_DATA
%token <int_info> RESERVE_SPACE
%token <arch_data> INSN PREFIX REG REGGROUP SEGREG TARGETMOD
%token LEFT_OP RIGHT_OP
%token <str_val> ID DIR_ID
%token LINE
%token DIR_2BYTE DIR_4BYTE DIR_ALIGN DIR_ASCII DIR_ASCIZ DIR_BALIGN
%token DIR_BSS DIR_BYTE DIR_COMM DIR_DATA DIR_DOUBLE DIR_ENDR DIR_EXTERN
%token DIR_EQU DIR_FILE DIR_FLOAT DIR_GLOBAL DIR_IDENT DIR_INT DIR_LOC
%token DIR_LCOMM DIR_OCTA DIR_ORG DIR_P2ALIGN DIR_REPT DIR_SECTION
%token DIR_SHORT DIR_SIZE DIR_SKIP DIR_STRING
%token DIR_TEXT DIR_TFLOAT DIR_TYPE DIR_QUAD DIR_WORD

%type <bc> line lineexp instr

%type <str_val> label_id
%type <ea> memaddr
%type <exp> expr regmemexpr
%type <sym> explabel
%type <valparams> strvals datavals strvals2 datavals2
%type <insn_operands> operands
%type <insn_operand> operand

%left '-' '+'
%left '|' '&' '^' '!'
%left '*' '/' '%' LEFT_OP RIGHT_OP
%nonassoc UNARYOP

%%
input: /* empty */
    | input line    {
	parser_gas->temp_bc =
	    yasm_section_bcs_append(parser_gas->cur_section, $2);
	if (parser_gas->temp_bc)
	    parser_gas->prev_bc = parser_gas->temp_bc;
	if (parser_gas->save_input)
	    yasm_linemap_add_source(parser_gas->linemap,
		parser_gas->temp_bc,
		parser_gas->save_line[parser_gas->save_last ^ 1]);
	yasm_linemap_goto_next(parser_gas->linemap);
    }
;

line: '\n'		{ $$ = (yasm_bytecode *)NULL; }
    | lineexp '\n'
    | error '\n'	{
	yasm__error(cur_line,
		    N_("label or instruction expected at start of line"));
	$$ = (yasm_bytecode *)NULL;
	yyerrok;
    }
;

lineexp: instr
    | label_id ':'		{
	$$ = (yasm_bytecode *)NULL;
	define_label(parser_gas, $1, 0);
    }
    | label_id ':' instr	{
	$$ = $3;
	define_label(parser_gas, $1, 0);
    }
    /* Alignment directives */
    | DIR_ALIGN datavals2 {
	/* FIXME: Whether this is power-of-two or not depends on arch */
	/*@dependent@*/ yasm_valparam *bound, *fill = NULL, *maxskip = NULL;
	bound = yasm_vps_first(&$2);
	if (bound)
	    fill = yasm_vps_next(bound);
	else
	    yasm__error(cur_line,
			N_("align directive must specify alignment"));
	if (fill)
	    maxskip = yasm_vps_next(fill);
	$$ = (yasm_bytecode *)NULL;
    }
    | DIR_P2ALIGN datavals2 {
	$$ = (yasm_bytecode *)NULL;
    }
    | DIR_BALIGN datavals2 {
	$$ = (yasm_bytecode *)NULL;
    }
    | DIR_ORG expr {
	$$ = (yasm_bytecode *)NULL;
    }
    /* Data visibility directives */
    | DIR_GLOBAL label_id {
	yasm_objfmt_global_declare(parser_gas->objfmt, $2, NULL, cur_line);
	yasm_xfree($2);
	$$ = NULL;
    }
    | DIR_COMM label_id ',' expr {
	yasm_objfmt_common_declare(parser_gas->objfmt, $2, $4, NULL, cur_line);
	yasm_xfree($2);
	$$ = NULL;
    }
    | DIR_COMM label_id ',' expr ',' expr {
	/* Give third parameter as objext valparam for use as alignment */
	yasm_valparamhead vps;
	yasm_valparam *vp;

	yasm_vps_initialize(&vps);
	vp = yasm_vp_create(NULL, $6);
	yasm_vps_append(&vps, vp);

	yasm_objfmt_common_declare(parser_gas->objfmt, $2, $4, &vps, cur_line);

	yasm_vps_delete(&vps);
	yasm_xfree($2);
	$$ = NULL;
    }
    | DIR_EXTERN label_id {
	/* Go ahead and do it, even though all undef become extern */
	yasm_objfmt_extern_declare(parser_gas->objfmt, $2, NULL, cur_line);
	yasm_xfree($2);
	$$ = NULL;
    }
    | DIR_LCOMM label_id ',' expr {
	/* Put into .bss section. */
	/*@dependent@*/ yasm_section *bss =
	    gas_get_section(parser_gas, yasm__xstrdup(".bss"), NULL, NULL);
	/* TODO: default alignment */
	yasm_symtab_define_label(p_symtab, $2, yasm_section_bcs_last(bss), 1,
				 cur_line);
	yasm_section_bcs_append(bss, yasm_bc_create_reserve($4, 1, cur_line));
	yasm_xfree($2);
	$$ = NULL;
    }
    | DIR_LCOMM label_id ',' expr ',' expr {
	/* Put into .bss section. */
	/*@dependent@*/ yasm_section *bss =
	    gas_get_section(parser_gas, yasm__xstrdup(".bss"), NULL, NULL);
	/* TODO: force alignment */
	yasm_symtab_define_label(p_symtab, $2, yasm_section_bcs_last(bss), 1,
				 cur_line);
	yasm_section_bcs_append(bss, yasm_bc_create_reserve($4, 1, cur_line));
	yasm_xfree($2);
	$$ = NULL;
    }
    /* Integer data definition directives */
    | DIR_ASCII strvals {
	$$ = gas_define_strings(parser_gas, &$2, 0);
	yasm_vps_delete(&$2);
    }
    | DIR_ASCIZ strvals {
	$$ = gas_define_strings(parser_gas, &$2, 1);
	yasm_vps_delete(&$2);
    }
    | DIR_BYTE datavals {
	$$ = gas_define_data(parser_gas, &$2, 1);
	yasm_vps_delete(&$2);
    }
    | DIR_SHORT datavals {
	/* TODO: This should depend on arch */
	$$ = gas_define_data(parser_gas, &$2, 2);
	yasm_vps_delete(&$2);
    }
    | DIR_WORD datavals {
	$$ = gas_define_data(parser_gas, &$2,
			     yasm_arch_wordsize(parser_gas->arch));
	yasm_vps_delete(&$2);
    }
    | DIR_INT datavals {
	/* TODO: This should depend on arch */
	$$ = gas_define_data(parser_gas, &$2, 4);
	yasm_vps_delete(&$2);
    }
    | DIR_2BYTE datavals {
	$$ = gas_define_data(parser_gas, &$2, 2);
	yasm_vps_delete(&$2);
    }
    | DIR_4BYTE datavals {
	$$ = gas_define_data(parser_gas, &$2, 4);
	yasm_vps_delete(&$2);
    }
    | DIR_QUAD datavals {
	$$ = gas_define_data(parser_gas, &$2, 8);
	yasm_vps_delete(&$2);
    }
    | DIR_OCTA datavals {
	$$ = gas_define_data(parser_gas, &$2, 16);
	yasm_vps_delete(&$2);
    }
    /* Floating point data definition directives */
    | DIR_FLOAT datavals {
	$$ = gas_define_data(parser_gas, &$2, 4);
	yasm_vps_delete(&$2);
    }
    | DIR_DOUBLE datavals {
	$$ = gas_define_data(parser_gas, &$2, 8);
	yasm_vps_delete(&$2);
    }
    | DIR_TFLOAT datavals {
	$$ = gas_define_data(parser_gas, &$2, 10);
	yasm_vps_delete(&$2);
    }
    /* Empty space / fill data definition directives */
    | DIR_SKIP expr {
	$$ = yasm_bc_create_reserve($2, 1, cur_line);
    }
    | DIR_SKIP expr ',' expr {
	yasm_datavalhead dvs;

	yasm_dvs_initialize(&dvs);
	yasm_dvs_append(&dvs, yasm_dv_create_expr($4));
	$$ = yasm_bc_create_data(&dvs, 1, cur_line);

	yasm_bc_set_multiple($$, $2);
    }
    /* Section directives */
    | DIR_TEXT {
	gas_switch_section(parser_gas, yasm__xstrdup(".text"), NULL, NULL);
	$$ = NULL;
    }
    | DIR_DATA {
	gas_switch_section(parser_gas, yasm__xstrdup(".data"), NULL, NULL);
	$$ = NULL;
    }
    | DIR_BSS {
	gas_switch_section(parser_gas, yasm__xstrdup(".bss"), NULL, NULL);
	$$ = NULL;
    }
    | DIR_SECTION label_id {
	gas_switch_section(parser_gas, $2, NULL, NULL);
	$$ = NULL;
    }
    | DIR_SECTION label_id ',' STRING {
	gas_switch_section(parser_gas, $2, $4, NULL);
	$$ = NULL;
    }
    | DIR_SECTION label_id ',' STRING ',' '@' label_id {
	gas_switch_section(parser_gas, $2, $4, $7);
	$$ = NULL;
    }
    /* Other directives */
    | DIR_IDENT strvals {
	/* Put text into .comment section. */
	/*@dependent@*/ yasm_section *comment =
	    gas_get_section(parser_gas, yasm__xstrdup(".comment"), NULL, NULL);
	/* To match GAS output, if the comment section is empty, put an
	 * initial 0 byte in the section.
	 */
	if (yasm_section_bcs_first(comment) == yasm_section_bcs_last(comment)) {
	    yasm_datavalhead dvs;

	    yasm_dvs_initialize(&dvs);
	    yasm_dvs_append(&dvs, yasm_dv_create_expr(
		p_expr_new_ident(yasm_expr_int(yasm_intnum_create_uint(0)))));
	    yasm_section_bcs_append(comment,
				    yasm_bc_create_data(&dvs, 1, cur_line));
	}
	yasm_section_bcs_append(comment,
				gas_define_strings(parser_gas, &$2, 1));
	yasm_vps_delete(&$2);
	$$ = NULL;
    }
    | DIR_FILE INTNUM STRING {
	/* TODO */
	$$ = NULL;
    }
    | DIR_LOC INTNUM INTNUM INTNUM {
	/* TODO */
	$$ = NULL;
    }
    | DIR_TYPE label_id ',' '@' label_id {
	/* TODO */
	$$ = NULL;
    }
    | DIR_SIZE label_id ',' expr {
	/* TODO */
	$$ = NULL;
    }
    | DIR_ID datavals	{
	yasm__warning(YASM_WARN_GENERAL, cur_line,
		      N_("directive `%s' not recognized"), $1);
	$$ = (yasm_bytecode *)NULL;
	yasm_xfree($1);
	yasm_vps_delete(&$2);
    }
    | DIR_ID error	{
	yasm__warning(YASM_WARN_GENERAL, cur_line,
		      N_("directive `%s' not recognized"), $1);
	$$ = (yasm_bytecode *)NULL;
	yasm_xfree($1);
    }
    | label_id '=' expr	{
	$$ = (yasm_bytecode *)NULL;
	yasm_symtab_define_equ(p_symtab, $1, $3, cur_line);
	yasm_xfree($1);
    }
;

instr: INSN		{
	$$ = yasm_bc_create_insn(parser_gas->arch, $1, 0, NULL, cur_line);
    }
    | INSN operands	{
	$$ = yasm_bc_create_insn(parser_gas->arch, $1, $2.num_operands,
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
    | ID {
	yasm__error(cur_line, N_("instruction not recognized: `%s'"), $1);
	$$ = NULL;
    }
    | ID operands {
	yasm__error(cur_line, N_("instruction not recognized: `%s'"), $1);
	$$ = NULL;
    }
    | ID error {
	yasm__error(cur_line, N_("instruction not recognized: `%s'"), $1);
	$$ = NULL;
    }
;

strvals: /* empty */	{ yasm_vps_initialize(&$$); }
    | strvals2
;

strvals2: STRING		{
	yasm_valparam *vp = yasm_vp_create($1, NULL);
	yasm_vps_initialize(&$$);
	yasm_vps_append(&$$, vp);
    }
    | strvals2 ',' STRING	{
	yasm_valparam *vp = yasm_vp_create($3, NULL);
	yasm_vps_append(&$1, vp);
	$$ = $1;
    }
    | strvals2 ',' ',' STRING	{
	yasm_valparam *vp = yasm_vp_create(NULL, NULL);
	yasm_vps_append(&$1, vp);
	vp = yasm_vp_create($4, NULL);
	yasm_vps_append(&$1, vp);
	$$ = $1;
    }
;

datavals: /* empty */	{ yasm_vps_initialize(&$$); }
    | datavals2
;

datavals2: expr			{
	yasm_valparam *vp = yasm_vp_create(NULL, $1);
	yasm_vps_initialize(&$$);
	yasm_vps_append(&$$, vp);
    }
    | datavals2 ',' expr	{
	yasm_valparam *vp = yasm_vp_create(NULL, $3);
	yasm_vps_append(&$1, vp);
	$$ = $1;
    }
    | datavals2 ',' ',' expr	{
	yasm_valparam *vp = yasm_vp_create(NULL, NULL);
	yasm_vps_append(&$1, vp);
	vp = yasm_vp_create(NULL, $4);
	yasm_vps_append(&$1, vp);
	$$ = $1;
    }
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

regmemexpr: '(' REG ')'	    {
	$$ = p_expr_new_ident(yasm_expr_reg($2[0]));
    }
    | '(' ',' REG ')'	    {
	$$ = p_expr_new(yasm_expr_reg($3[0]), YASM_EXPR_MUL,
			yasm_expr_int(yasm_intnum_create_uint(1)));
    }
    | '(' ',' INTNUM ')'    {
	if (yasm_intnum_get_uint($3) != 1)
	    yasm__warning(YASM_WARN_GENERAL, cur_line,
			  N_("scale factor of %u without an index register"),
			  yasm_intnum_get_uint($3));
	$$ = p_expr_new(yasm_expr_int(yasm_intnum_create_uint(0)),
			YASM_EXPR_MUL, yasm_expr_int($3));
    }
    | '(' REG ',' REG ')'  {
	$$ = p_expr_new(yasm_expr_reg($2[0]), YASM_EXPR_ADD,
	    yasm_expr_expr(p_expr_new(yasm_expr_reg($4[0]), YASM_EXPR_MUL,
		yasm_expr_int(yasm_intnum_create_uint(1)))));
    }
    | '(' ',' REG ',' INTNUM ')'  {
	$$ = p_expr_new(yasm_expr_reg($3[0]), YASM_EXPR_MUL,
			yasm_expr_int($5));
    }
    | '(' REG ',' REG ',' INTNUM ')'  {
	$$ = p_expr_new(yasm_expr_reg($2[0]), YASM_EXPR_ADD,
	    yasm_expr_expr(p_expr_new(yasm_expr_reg($4[0]), YASM_EXPR_MUL,
				      yasm_expr_int($6))));
    }
;

/* memory addresses */
memaddr: expr		    {
	$$ = yasm_arch_ea_create(parser_gas->arch, $1);
    }
    | regmemexpr	    {
	$$ = yasm_arch_ea_create(parser_gas->arch, $1);
	yasm_ea_set_strong($$, 1);
    }
    | expr regmemexpr	    {
	$$ = yasm_arch_ea_create(parser_gas->arch,
				 p_expr_new_tree($2, YASM_EXPR_ADD, $1));
	yasm_ea_set_strong($$, 1);
    }
    | SEGREG ':' memaddr  {
	$$ = $3;
	yasm_ea_set_segreg($$, $1[0], cur_line);
    }
;

operand: memaddr	    { $$ = yasm_operand_create_mem($1); }
    | REG		    { $$ = yasm_operand_create_reg($1[0]); }
    | REGGROUP		    { $$ = yasm_operand_create_reg($1[0]); }
    | REGGROUP '(' INTNUM ')'	{
	unsigned long reg =
	    yasm_arch_reggroup_get_reg(parser_gas->arch, $1[0],
				       yasm_intnum_get_uint($3));
	yasm_intnum_destroy($3);
	if (reg == 0) {
	    yasm__error(cur_line, N_("bad register index `%u'"),
			yasm_intnum_get_uint($3));
	    $$ = yasm_operand_create_reg($1[0]);
	} else
	    $$ = yasm_operand_create_reg(reg);
    }
    | '$' expr		    { $$ = yasm_operand_create_imm($2); }
    | '*' REG		    {
	$$ = yasm_operand_create_reg($2[0]);
	$$->deref = 1;
    }
    | '*' memaddr	    {
	$$ = yasm_operand_create_mem($2);
	$$->deref = 1;
    }
;

/* Expressions */
expr: INTNUM		{ $$ = p_expr_new_ident(yasm_expr_int($1)); }
    | FLTNUM		{ $$ = p_expr_new_ident(yasm_expr_float($1)); }
    | explabel		{ $$ = p_expr_new_ident(yasm_expr_sym($1)); }
    | expr '|' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_OR, $3); }
    | expr '^' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_XOR, $3); }
    | expr '&' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_AND, $3); }
    | expr '!' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_NOR, $3); }
    | expr LEFT_OP expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SHL, $3); }
    | expr RIGHT_OP expr { $$ = p_expr_new_tree($1, YASM_EXPR_SHR, $3); }
    | expr '+' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_ADD, $3); }
    | expr '-' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_SUB, $3); }
    | expr '*' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_MUL, $3); }
    | expr '/' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_DIV, $3); }
    | expr '%' expr	{ $$ = p_expr_new_tree($1, YASM_EXPR_MOD, $3); }
    | '+' expr %prec UNARYOP	{ $$ = $2; }
    | '-' expr %prec UNARYOP	{ $$ = p_expr_new_branch(YASM_EXPR_NEG, $2); }
    | '~' expr %prec UNARYOP	{ $$ = p_expr_new_branch(YASM_EXPR_NOT, $2); }
    | '(' expr ')'	{ $$ = $2; }
;

explabel: label_id	{
	/* "." references the current assembly position */
	if ($1[1] == '\0' && $1[0] == '.')
	    $$ = yasm_symtab_define_label(p_symtab, ".", parser_gas->prev_bc,
					  0, cur_line);
	else
	    $$ = yasm_symtab_use(p_symtab, $1, cur_line);
	yasm_xfree($1);
    }
    | label_id '@' label_id {
	/* TODO: this is needed for shared objects, e.g. sym@PLT */
	$$ = yasm_symtab_use(p_symtab, $1, cur_line);
	yasm_xfree($1);
	yasm_xfree($3);
    }
;

label_id: ID | DIR_ID;

%%
/*@=usedef =nullassign =memtrans =usereleased =compdef =mustfree@*/

#undef parser_gas

static void
define_label(yasm_parser_gas *parser_gas, char *name, int local)
{
    if (!local) {
	if (parser_gas->locallabel_base)
	    yasm_xfree(parser_gas->locallabel_base);
	parser_gas->locallabel_base_len = strlen(name);
	parser_gas->locallabel_base =
	    yasm_xmalloc(parser_gas->locallabel_base_len+1);
	strcpy(parser_gas->locallabel_base, name);
    }

    yasm_symtab_define_label(p_symtab, name, parser_gas->prev_bc, 1,
			     cur_line);
    yasm_xfree(name);
}

static yasm_section *
gas_get_section(yasm_parser_gas *parser_gas, char *name,
		/*@null@*/ char *flags, /*@null@*/ char *type)
{
    yasm_valparamhead vps;
    yasm_valparam *vp;
    char *gasflags;
    yasm_section *new_section;

    yasm_vps_initialize(&vps);
    vp = yasm_vp_create(name, NULL);
    yasm_vps_append(&vps, vp);

    if (flags) {
	gasflags = yasm_xmalloc(5+strlen(flags));
	strcpy(gasflags, "gas_");
	strcat(gasflags, flags);
    } else
	gasflags = yasm__xstrdup("gas_");
    vp = yasm_vp_create(gasflags, NULL);
    yasm_vps_append(&vps, vp);

    if (type) {
	vp = yasm_vp_create(type, NULL);
	yasm_vps_append(&vps, vp);
    }

    new_section = yasm_objfmt_section_switch(parser_gas->objfmt, &vps, NULL,
					     cur_line);

    yasm_vps_delete(&vps);
    return new_section;
}

static void
gas_switch_section(yasm_parser_gas *parser_gas, char *name,
		   /*@null@*/ char *flags, /*@null@*/ char *type)
{
    yasm_section *new_section;

    new_section = gas_get_section(parser_gas, name, flags, type);
    if (new_section) {
	parser_gas->cur_section = new_section;
	parser_gas->prev_bc = yasm_section_bcs_last(new_section);
    } else
	yasm__error(cur_line, N_("invalid section name `%s'"), name);
}

static yasm_bytecode *
gas_define_strings(yasm_parser_gas *parser_gas, yasm_valparamhead *vps,
		   int withzero)
{
    if (yasm_vps_first(vps)) {
	yasm_datavalhead dvs;
	yasm_valparam *cur;

	yasm_dvs_initialize(&dvs);
	yasm_vps_foreach(cur, vps) {
	    if (!cur->val)
		yasm__error(cur_line, N_("missing string value"));
	    else {
		yasm_dvs_append(&dvs, yasm_dv_create_string(cur->val));
		cur->val = NULL;
		if (withzero)
		    yasm_dvs_append(&dvs, yasm_dv_create_expr(
			p_expr_new_ident(yasm_expr_int(
			    yasm_intnum_create_uint(0)))));
	    }
	}
	return yasm_bc_create_data(&dvs, 1, cur_line);
    } else
	return NULL;
}

static yasm_bytecode *
gas_define_data(yasm_parser_gas *parser_gas, yasm_valparamhead *vps,
		unsigned int size)
{
    if (yasm_vps_first(vps)) {
	yasm_datavalhead dvs;
	yasm_valparam *cur;

	yasm_dvs_initialize(&dvs);
	yasm_vps_foreach(cur, vps) {
	    if (!cur->param)
		yasm__error(cur_line, N_("missing data value"));
	    else
		yasm_dvs_append(&dvs, yasm_dv_create_expr(cur->param));
	    cur->param = NULL;
	}
	return yasm_bc_create_data(&dvs, size, cur_line);
    } else
	return NULL;
}

static void
gas_parser_directive(yasm_parser_gas *parser_gas, const char *name,
		      yasm_valparamhead *valparams,
		      yasm_valparamhead *objext_valparams)
{
    unsigned long line = cur_line;

    /* Handle (mostly) output-format independent directives here */
    if (!yasm_arch_parse_directive(parser_gas->arch, name, valparams,
		    objext_valparams, parser_gas->object, line)) {
	;
    } else if (yasm_objfmt_directive(parser_gas->objfmt, name, valparams,
				     objext_valparams, line)) {
	yasm__error(line, N_("unrecognized directive [%s]"), name);
    }

    yasm_vps_delete(valparams);
    if (objext_valparams)
	yasm_vps_delete(objext_valparams);
}
