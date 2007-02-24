/*
 * GAS-compatible parser
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
#define YASM_EXPR_INTERNAL
#include <libyasm.h>

#include <limits.h>
#include <math.h>

#include "modules/parsers/gas/gas-parser.h"

static yasm_bytecode *parse_line(yasm_parser_gas *parser_gas);
static yasm_bytecode *parse_instr(yasm_parser_gas *parser_gas);
static int parse_dirvals(yasm_parser_gas *parser_gas, yasm_valparamhead *vps);
static int parse_dirstrvals(yasm_parser_gas *parser_gas,
			    yasm_valparamhead *vps);
static int parse_datavals(yasm_parser_gas *parser_gas, yasm_datavalhead *dvs);
static int parse_strvals(yasm_parser_gas *parser_gas, yasm_datavalhead *dvs);
static yasm_effaddr *parse_memaddr(yasm_parser_gas *parser_gas);
static yasm_insn_operand *parse_operand(yasm_parser_gas *parser_gas);
static yasm_expr *parse_expr(yasm_parser_gas *parser_gas);
static yasm_expr *parse_expr0(yasm_parser_gas *parser_gas);
static yasm_expr *parse_expr1(yasm_parser_gas *parser_gas);
static yasm_expr *parse_expr2(yasm_parser_gas *parser_gas);

static void define_label(yasm_parser_gas *parser_gas, char *name, int local);
static void define_lcomm(yasm_parser_gas *parser_gas, /*@only@*/ char *name,
			 yasm_expr *size, /*@null@*/ yasm_expr *align);
static yasm_section *gas_get_section
    (yasm_parser_gas *parser_gas, /*@only@*/ char *name, /*@null@*/ char *flags,
     /*@null@*/ char *type, /*@null@*/ yasm_valparamhead *objext_valparams,
     int builtin);
static void gas_switch_section
    (yasm_parser_gas *parser_gas, /*@only@*/ char *name, /*@null@*/ char *flags,
     /*@null@*/ char *type, /*@null@*/ yasm_valparamhead *objext_valparams,
     int builtin);
static yasm_bytecode *gas_parser_align
    (yasm_parser_gas *parser_gas, yasm_section *sect, yasm_expr *boundval,
     /*@null@*/ yasm_expr *fillval, /*@null@*/ yasm_expr *maxskipval,
     int power2);
static yasm_bytecode *gas_parser_dir_fill
    (yasm_parser_gas *parser_gas, /*@only@*/ yasm_expr *repeat,
     /*@only@*/ /*@null@*/ yasm_expr *size,
     /*@only@*/ /*@null@*/ yasm_expr *value);
static void gas_parser_directive
    (yasm_parser_gas *parser_gas, const char *name,
     yasm_valparamhead *valparams,
     /*@null@*/ yasm_valparamhead *objext_valparams);

#define is_eol_tok(tok)	((tok) == '\n' || (tok) == ';' || (tok) == 0)
#define is_eol()	is_eol_tok(curtok)

#define get_next_token()    (curtok = gas_parser_lex(&curval, parser_gas))

static void
get_peek_token(yasm_parser_gas *parser_gas)
{
    char savech = parser_gas->tokch;
    if (parser_gas->peek_token != NONE)
	yasm_internal_error(N_("only can have one token of lookahead"));
    parser_gas->peek_token =
	gas_parser_lex(&parser_gas->peek_tokval, parser_gas);
    parser_gas->peek_tokch = parser_gas->tokch;
    parser_gas->tokch = savech;
}

static void
destroy_curtok_(yasm_parser_gas *parser_gas)
{
    if (curtok < 256)
	;
    else switch ((enum tokentype)curtok) {
	case INTNUM:
	    yasm_intnum_destroy(curval.intn);
	    break;
	case FLTNUM:
	    yasm_floatnum_destroy(curval.flt);
	    break;
	case ID:
	case LABEL:
	    yasm_xfree(curval.str_val);
	    break;
	case STRING:
	    yasm_xfree(curval.str.contents);
	    break;
	default:
	    break;
    }
    curtok = NONE;	    /* sanity */
}
#define destroy_curtok()    destroy_curtok_(parser_gas)

/* Eat all remaining tokens to EOL, discarding all of them.  If there's any
 * intervening tokens, generates an error (junk at end of line).
 */
static void
demand_eol_(yasm_parser_gas *parser_gas)
{
    if (is_eol())
	return;

    yasm_error_set(YASM_ERROR_SYNTAX,
	N_("junk at end of line, first unrecognized character is `%c'"),
	parser_gas->tokch);

    do {
	destroy_curtok();
	get_next_token();
    } while (!is_eol());
}
#define demand_eol() demand_eol_(parser_gas)

static int
expect_(yasm_parser_gas *parser_gas, int token)
{
    static char strch[] = "expected ` '";
    const char *str;

    if (curtok == token)
	return 1;

    switch (token) {
	case INTNUM:		str = "expected integer"; break;
	case FLTNUM:		str = "expected floating point value"; break;
	case STRING:		str = "expected string"; break;
	case INSN:		str = "expected instruction"; break;
	case PREFIX:		str = "expected instruction prefix"; break;
	case REG:		str = "expected register"; break;
	case REGGROUP:		str = "expected register group"; break;
	case SEGREG:		str = "expected segment register"; break;
	case TARGETMOD:		str = "expected target modifier"; break;
	case LEFT_OP:		str = "expected <<"; break;
	case RIGHT_OP:		str = "expected >>"; break;
	case ID:		str = "expected identifier"; break;
	case LABEL:		str = "expected label"; break;
	case LINE:
	case DIR_ALIGN:
	case DIR_ASCII:
	case DIR_COMM:
	case DIR_DATA:
	case DIR_ENDR:
	case DIR_EXTERN:
	case DIR_EQU:
	case DIR_FILE:
	case DIR_FILL:
	case DIR_GLOBAL:
	case DIR_IDENT:
	case DIR_LEB128:
	case DIR_LINE:
	case DIR_LOC:
	case DIR_LOCAL:
	case DIR_LCOMM:
	case DIR_ORG:
	case DIR_REPT:
	case DIR_SECTION:
	case DIR_SECTNAME:
	case DIR_SIZE:
	case DIR_SKIP:
	case DIR_TYPE:
	case DIR_WEAK:
	case DIR_ZERO:
	    str = "expected directive";
	    break;
	default:
	    strch[10] = token;
	    str = strch;
	    break;
    }
    yasm_error_set(YASM_ERROR_PARSE, str);
    destroy_curtok();
    return 0;
}
#define expect(token) expect_(parser_gas, token)

void
gas_parser_parse(yasm_parser_gas *parser_gas)
{
    while (get_next_token() != 0) {
	yasm_bytecode *bc = NULL, *temp_bc;
	
	if (!is_eol()) {
	    bc = parse_line(parser_gas);
	    demand_eol();
	}

	yasm_errwarn_propagate(parser_gas->errwarns, cur_line);

	temp_bc = yasm_section_bcs_append(parser_gas->cur_section, bc);
	if (temp_bc)
	    parser_gas->prev_bc = temp_bc;
	if (curtok == ';')
	    continue;	    /* don't advance line number until \n */
	if (parser_gas->save_input)
	    yasm_linemap_add_source(parser_gas->linemap,
		temp_bc,
		(char *)parser_gas->save_line[parser_gas->save_last ^ 1]);
	yasm_linemap_goto_next(parser_gas->linemap);
	parser_gas->dir_line++;	/* keep track for .line followed by .file */
    }
}

static yasm_bytecode *
parse_line(yasm_parser_gas *parser_gas)
{
    yasm_bytecode *bc;
    yasm_expr *e;
    yasm_intnum *intn;
    yasm_datavalhead dvs;
    yasm_valparamhead vps;
    yasm_valparam *vp;
    unsigned int ival;
    char *id;

    if (is_eol())
	return NULL;

    bc = parse_instr(parser_gas);
    if (bc)
	return bc;

    switch (curtok) {
	case ID:
	    id = ID_val;
	    get_next_token(); /* ID */
	    if (curtok == ':') {
		/* Label */
		get_next_token(); /* : */
		define_label(parser_gas, id, 0);
		return parse_line(parser_gas);
	    } else if (curtok == '=') {
		/* EQU */
		get_next_token(); /* = */
		e = parse_expr(parser_gas);
		if (e)
		    yasm_symtab_define_equ(p_symtab, id, e, cur_line);
		else
		    yasm_error_set(YASM_ERROR_SYNTAX,
				   N_("expression expected after `%s'"), "=");
		yasm_xfree(id);
		return NULL;
	    }
	    /* must be an error at this point */
	    if (id[0] == '.')
		yasm_warn_set(YASM_WARN_GENERAL,
			      N_("directive `%s' not recognized"), id);
	    else
		yasm_error_set(YASM_ERROR_SYNTAX,
			       N_("instruction not recognized: `%s'"), id);
	    yasm_xfree(id);
	    return NULL;
	case LABEL:
	    define_label(parser_gas, LABEL_val, 0);
	    get_next_token(); /* LABEL */
	    return parse_line(parser_gas);

	/* Line directive */
	case DIR_LINE:
	    get_next_token(); /* DIR_LINE */

	    if (!expect(INTNUM)) return NULL;
	    if (yasm_intnum_sign(INTNUM_val) < 0) {
		get_next_token(); /* INTNUM */
		yasm_error_set(YASM_ERROR_SYNTAX,
			       N_("line number is negative"));
		return NULL;
	    }

	    parser_gas->dir_line = yasm_intnum_get_uint(INTNUM_val);
	    yasm_intnum_destroy(INTNUM_val);
	    get_next_token(); /* INTNUM */
 
	    if (parser_gas->dir_fileline == 3) {
		/* Have both file and line */
		yasm_linemap_set(parser_gas->linemap, NULL,
				 parser_gas->dir_line, 1);
	    } else if (parser_gas->dir_fileline == 1) {
		/* Had previous file directive only */
		parser_gas->dir_fileline = 3;
		yasm_linemap_set(parser_gas->linemap, parser_gas->dir_file,
				 parser_gas->dir_line, 1);
	    } else {
		/* Didn't see file yet */
		parser_gas->dir_fileline = 2;
	    }
	    return NULL;

	/* Macro directives */
	case DIR_REPT:
	    get_next_token(); /* DIR_REPT */
	    e = parse_expr(parser_gas);
	    if (!e) {
		yasm_error_set(YASM_ERROR_SYNTAX,
			       N_("expression expected after `%s'"),
			       ".rept");
		return NULL;
	    }
	    intn = yasm_expr_get_intnum(&e, 0);

	    if (!intn) {
		yasm_error_set(YASM_ERROR_NOT_ABSOLUTE,
			       N_("rept expression not absolute"));
	    } else if (yasm_intnum_sign(intn) < 0) {
		yasm_error_set(YASM_ERROR_VALUE,
			       N_("rept expression is negative"));
	    } else {
		gas_rept *rept = yasm_xmalloc(sizeof(gas_rept));
		STAILQ_INIT(&rept->lines);
		rept->startline = cur_line;
		rept->numrept = yasm_intnum_get_uint(intn);
		rept->numdone = 0;
		rept->line = NULL;
		rept->linepos = 0;
		rept->ended = 0;
		rept->oldbuf = NULL;
		rept->oldbuflen = 0;
		rept->oldbufpos = 0;
		parser_gas->rept = rept;
	    }
	    return NULL;
	case DIR_ENDR:
	    get_next_token(); /* DIR_ENDR */
	    /* Shouldn't ever get here unless we didn't get a DIR_REPT first */
	    yasm_error_set(YASM_ERROR_SYNTAX, N_("endr without matching rept"));
	    return NULL;

	/* Alignment directives */
	case DIR_ALIGN:
	{
	    yasm_expr *bound, *fill=NULL, *maxskip=NULL;

	    ival = DIR_ALIGN_val;
	    get_next_token(); /* DIR_ALIGN */

	    bound = parse_expr(parser_gas);
	    if (!bound) {
		yasm_error_set(YASM_ERROR_SYNTAX,
			       N_(".align directive must specify alignment"));
		return NULL;
	    }

	    if (curtok == ',') {
		get_next_token(); /* ',' */
		fill = parse_expr(parser_gas);
		if (curtok == ',') {
		    get_next_token(); /* ',' */
		    maxskip = parse_expr(parser_gas);
		}
	    }

	    return gas_parser_align(parser_gas, parser_gas->cur_section, bound,
				    fill, maxskip, (int)ival);
	}
	case DIR_ORG:
	    get_next_token(); /* DIR_ORG */
	    if (!expect(INTNUM)) return NULL;
	    /* TODO: support expr instead of intnum */
	    bc = yasm_bc_create_org(yasm_intnum_get_uint(INTNUM_val), cur_line);
	    yasm_intnum_destroy(INTNUM_val);
	    get_next_token(); /* INTNUM */
	    return bc;
    
	/* Data visibility directives */
	case DIR_LOCAL:
	    get_next_token(); /* DIR_LOCAL */
	    if (!expect(ID)) return NULL;
	    yasm_symtab_declare(parser_gas->symtab, ID_val, YASM_SYM_DLOCAL,
				cur_line);
	    yasm_xfree(ID_val);
	    get_next_token(); /* ID */
	    return NULL;
	case DIR_GLOBAL:
	    get_next_token(); /* DIR_GLOBAL */
	    if (!expect(ID)) return NULL;
	    yasm_objfmt_global_declare(parser_gas->objfmt, ID_val, NULL,
				       cur_line);
	    yasm_xfree(ID_val);
	    get_next_token(); /* ID */
	    return NULL;
	case DIR_COMM:
	case DIR_LCOMM:
	{
	    yasm_expr *align = NULL;
	    /*@null@*/ /*@dependent@*/ yasm_symrec *sym;
	    int is_lcomm = curtok == DIR_LCOMM;

	    get_next_token(); /* DIR_LOCAL */

	    if (!expect(ID)) return NULL;
	    id = ID_val;
	    get_next_token(); /* ID */
	    if (!expect(',')) {
		yasm_xfree(id);
		return NULL;
	    }
	    get_next_token(); /* , */
	    e = parse_expr(parser_gas);
	    if (!e) {
		yasm_error_set(YASM_ERROR_SYNTAX, N_("size expected for `%s'"),
			       ".COMM");
		return NULL;
	    }
	    if (curtok == ',') {
		/* Optional alignment expression */
		get_next_token(); /* ',' */
		align = parse_expr(parser_gas);
	    }
	    /* If already explicitly declared local, treat like LCOMM */
	    if (is_lcomm
		|| ((sym = yasm_symtab_get(parser_gas->symtab, id))
		    && yasm_symrec_get_visibility(sym) == YASM_SYM_DLOCAL)) {
		define_lcomm(parser_gas, id, e, align);
	    } else if (align) {
		/* Give third parameter as objext valparam */
		yasm_vps_initialize(&vps);
		vp = yasm_vp_create(NULL, align);
		yasm_vps_append(&vps, vp);

		yasm_objfmt_common_declare(parser_gas->objfmt, id, e, &vps,
					   cur_line);

		yasm_vps_delete(&vps);
		yasm_xfree(id);
	    } else {
		yasm_objfmt_common_declare(parser_gas->objfmt, id, e, NULL,
					   cur_line);
		yasm_xfree(id);
	    }
	    return NULL;
	}
	case DIR_EXTERN:
	    get_next_token(); /* DIR_EXTERN */
	    if (!expect(ID)) return NULL;
	    /* Go ahead and do it, even though all undef become extern */
	    yasm_objfmt_extern_declare(parser_gas->objfmt, ID_val, NULL,
				       cur_line);
	    yasm_xfree(ID_val);
	    get_next_token(); /* ID */
	    return NULL;
	case DIR_WEAK:
	    get_next_token(); /* DIR_EXTERN */
	    if (!expect(ID)) return NULL;

	    yasm_vps_initialize(&vps);
	    vp = yasm_vp_create(ID_val, NULL);
	    yasm_vps_append(&vps, vp);
	    get_next_token(); /* ID */

	    yasm_objfmt_directive(parser_gas->objfmt, "weak", &vps, NULL,
				  cur_line);

	    yasm_vps_delete(&vps);
	    return NULL;

	/* Integer data definition directives */
	case DIR_ASCII:
	    ival = DIR_ASCII_val;
	    get_next_token(); /* DIR_ASCII */
	    if (!parse_strvals(parser_gas, &dvs))
		return NULL;
	    return yasm_bc_create_data(&dvs, 1, (int)ival, parser_gas->arch,
				       cur_line);
	case DIR_DATA:
	    ival = DIR_DATA_val;
	    get_next_token(); /* DIR_DATA */
	    if (!parse_datavals(parser_gas, &dvs))
		return NULL;
	    return yasm_bc_create_data(&dvs, ival, 0, parser_gas->arch,
				       cur_line);
	case DIR_LEB128:
	    ival = DIR_LEB128_val;
	    get_next_token(); /* DIR_LEB128 */
	    if (!parse_datavals(parser_gas, &dvs))
		return NULL;
	    return yasm_bc_create_leb128(&dvs, (int)ival, cur_line);

	/* Empty space / fill data definition directives */
	case DIR_ZERO:
	    get_next_token(); /* DIR_ZERO */
	    e = parse_expr(parser_gas);
	    if (!e) {
		yasm_error_set(YASM_ERROR_SYNTAX,
			       N_("expression expected after `%s'"), ".ZERO");
		return NULL;
	    }

	    yasm_dvs_initialize(&dvs);
	    yasm_dvs_append(&dvs, yasm_dv_create_expr(
		p_expr_new_ident(yasm_expr_int(yasm_intnum_create_uint(0)))));
	    bc = yasm_bc_create_data(&dvs, 1, 0, parser_gas->arch, cur_line);
	    yasm_bc_set_multiple(bc, e);
	    return bc;
	case DIR_SKIP:
	{
	    yasm_expr *e_val;

	    get_next_token(); /* DIR_SKIP */
	    e = parse_expr(parser_gas);
	    if (!e) {
		yasm_error_set(YASM_ERROR_SYNTAX,
			       N_("expression expected after `%s'"), ".SKIP");
		return NULL;
	    }
	    if (curtok != ',')
		return yasm_bc_create_reserve(e, 1, cur_line);
	    get_next_token(); /* ',' */
	    e_val = parse_expr(parser_gas);
	    yasm_dvs_initialize(&dvs);
	    yasm_dvs_append(&dvs, yasm_dv_create_expr(e_val));
	    bc = yasm_bc_create_data(&dvs, 1, 0, parser_gas->arch, cur_line);

	    yasm_bc_set_multiple(bc, e);
	    return bc;
	}

	/* fill data definition directive */
	case DIR_FILL:
	{
	    yasm_expr *sz=NULL, *val=NULL;
	    get_next_token(); /* DIR_FILL */
	    e = parse_expr(parser_gas);
	    if (!e) {
		yasm_error_set(YASM_ERROR_SYNTAX,
			       N_("expression expected after `%s'"), ".FILL");
		return NULL;
	    }
	    if (curtok == ',') {
		get_next_token(); /* ',' */
		sz = parse_expr(parser_gas);
		if (curtok == ',') {
		    get_next_token(); /* ',' */
		    val = parse_expr(parser_gas);
		}
	    }
	    return gas_parser_dir_fill(parser_gas, e, sz, val);
	}

	/* Section directives */
	case DIR_SECTNAME:
	    gas_switch_section(parser_gas, DIR_SECTNAME_val, NULL, NULL, NULL,
			       1);
	    get_next_token(); /* DIR_SECTNAME */
	    return NULL;
	case DIR_SECTION:
	{
	    /* DIR_SECTION ID ',' STRING ',' '@' ID ',' dirvals */
	    char *sectname, *flags = NULL, *type = NULL;
	    int have_vps = 0;

	    get_next_token(); /* DIR_SECTION */

	    if (!expect(ID)) return NULL;
	    sectname = ID_val;
	    get_next_token(); /* ID */

	    if (curtok == ',') {
		get_next_token(); /* ',' */
		if (!expect(STRING)) {
		    yasm_error_set(YASM_ERROR_SYNTAX,
				   N_("flag string expected"));
		    yasm_xfree(sectname);
		    return NULL;
		}
		flags = STRING_val.contents;
		get_next_token(); /* STRING */
	    }

	    if (curtok == ',') {
		get_next_token(); /* ',' */
		if (!expect('@')) {
		    yasm_xfree(sectname);
		    yasm_xfree(flags);
		    return NULL;
		}
		get_next_token(); /* '@' */
		if (!expect(ID)) {
		    yasm_xfree(sectname);
		    yasm_xfree(flags);
		    return NULL;
		}
		type = ID_val;
		get_next_token(); /* ID */
	    }

	    if (curtok == ',') {
		get_next_token(); /* ',' */
		if (parse_dirvals(parser_gas, &vps))
		    have_vps = 1;
	    }

	    gas_switch_section(parser_gas, sectname, flags, type,
			       have_vps ? &vps : NULL, 0);
	    yasm_xfree(flags);
	    return NULL;
	}

	/* Other directives */
	case DIR_IDENT:
	    get_next_token(); /* DIR_IDENT */
	    if (!parse_dirstrvals(parser_gas, &vps))
		return NULL;
	    yasm_objfmt_directive(parser_gas->objfmt, "ident", &vps, NULL,
				  cur_line);
	    yasm_vps_delete(&vps);
	    return NULL;
	case DIR_FILE:
	    get_next_token(); /* DIR_FILE */
	    if (curtok == STRING) {
		/* No file number; this form also sets the assembler's
		 * internal line number.
		 */
		char *filename = STRING_val.contents;
		get_next_token(); /* STRING */
		if (parser_gas->dir_fileline == 3) {
		    /* Have both file and line */
		    const char *old_fn;
		    unsigned long old_line;

		    yasm_linemap_lookup(parser_gas->linemap, cur_line, &old_fn,
					&old_line);
		    yasm_linemap_set(parser_gas->linemap, filename, old_line,
				     1);
		} else if (parser_gas->dir_fileline == 2) {
		    /* Had previous line directive only */
		    parser_gas->dir_fileline = 3;
		    yasm_linemap_set(parser_gas->linemap, filename,
				     parser_gas->dir_line, 1);
		} else {
		    /* Didn't see line yet, save file */
		    parser_gas->dir_fileline = 1;
		    if (parser_gas->dir_file)
			yasm_xfree(parser_gas->dir_file);
		    parser_gas->dir_file = yasm__xstrdup(filename);
		}

		/* Pass change along to debug format */
		yasm_vps_initialize(&vps);
		vp = yasm_vp_create(filename, NULL);
		yasm_vps_append(&vps, vp);

		yasm_dbgfmt_directive(parser_gas->dbgfmt, "file",
				      parser_gas->cur_section, &vps, cur_line);

		yasm_vps_delete(&vps);
		return NULL;
	    }

	    /* fileno filename form */
	    yasm_vps_initialize(&vps);

	    if (!expect(INTNUM)) return NULL;
	    vp = yasm_vp_create(NULL,
				p_expr_new_ident(yasm_expr_int(INTNUM_val)));
	    yasm_vps_append(&vps, vp);
	    get_next_token(); /* INTNUM */

	    if (!expect(STRING)) {
		yasm_vps_delete(&vps);
		return NULL;
	    }
	    vp = yasm_vp_create(STRING_val.contents, NULL);
	    yasm_vps_append(&vps, vp);
	    get_next_token(); /* STRING */

	    yasm_dbgfmt_directive(parser_gas->dbgfmt, "file",
				  parser_gas->cur_section, &vps, cur_line);

	    yasm_vps_delete(&vps);
	    return NULL;
	case DIR_LOC:
	    /* INTNUM INTNUM INTNUM */
	    get_next_token(); /* DIR_LOC */
	    yasm_vps_initialize(&vps);

	    if (!expect(INTNUM)) return NULL;
	    vp = yasm_vp_create(NULL,
				p_expr_new_ident(yasm_expr_int(INTNUM_val)));
	    yasm_vps_append(&vps, vp);
	    get_next_token(); /* INTNUM */

	    if (!expect(INTNUM)) {
		yasm_vps_delete(&vps);
		return NULL;
	    }
	    vp = yasm_vp_create(NULL,
				p_expr_new_ident(yasm_expr_int(INTNUM_val)));
	    yasm_vps_append(&vps, vp);
	    get_next_token(); /* INTNUM */

	    if (!expect(INTNUM)) {
		yasm_vps_delete(&vps);
		return NULL;
	    }
	    vp = yasm_vp_create(NULL,
				p_expr_new_ident(yasm_expr_int(INTNUM_val)));
	    yasm_vps_append(&vps, vp);
	    get_next_token(); /* INTNUM */

	    yasm_dbgfmt_directive(parser_gas->dbgfmt, "loc",
				  parser_gas->cur_section, &vps, cur_line);

	    yasm_vps_delete(&vps);
	    return NULL;
	case DIR_TYPE:
	    /* ID ',' '@' ID */
	    get_next_token(); /* DIR_TYPE */
	    yasm_vps_initialize(&vps);

	    if (!expect(ID)) return NULL;
	    vp = yasm_vp_create(ID_val, NULL);
	    yasm_vps_append(&vps, vp);
	    get_next_token(); /* ID */

	    if (!expect(',')) {
		yasm_vps_delete(&vps);
		return NULL;
	    }
	    get_next_token(); /* ',' */

	    if (!expect('@')) {
		yasm_vps_delete(&vps);
		return NULL;
	    }
	    get_next_token(); /* '@' */

	    if (!expect(ID)) {
		yasm_vps_delete(&vps);
		return NULL;
	    }
	    vp = yasm_vp_create(ID_val, NULL);
	    yasm_vps_append(&vps, vp);
	    get_next_token(); /* ID */

	    yasm_objfmt_directive(parser_gas->objfmt, "type", &vps, NULL,
				  cur_line);

	    yasm_vps_delete(&vps);
	    return NULL;
	case DIR_SIZE:
	    /* ID ',' expr */
	    get_next_token(); /* DIR_SIZE */
	    yasm_vps_initialize(&vps);

	    if (!expect(ID)) return NULL;
	    vp = yasm_vp_create(ID_val, NULL);
	    yasm_vps_append(&vps, vp);
	    get_next_token(); /* ID */

	    if (!expect(',')) {
		yasm_vps_delete(&vps);
		return NULL;
	    }
	    get_next_token(); /* ',' */

	    e = parse_expr(parser_gas);
	    if (!e) {
		yasm_error_set(YASM_ERROR_SYNTAX,
			       N_("expression expected for `.size'"));
		yasm_vps_delete(&vps);
		return NULL;
	    }
	    vp = yasm_vp_create(NULL, e);
	    yasm_vps_append(&vps, vp);

	    yasm_objfmt_directive(parser_gas->objfmt, "size", &vps, NULL,
				  cur_line);

	    yasm_vps_delete(&vps);
	    return NULL;
	default:
	    yasm_error_set(YASM_ERROR_SYNTAX,
		N_("label or instruction expected at start of line"));
	    return NULL;
    }
}

static yasm_bytecode *
parse_instr(yasm_parser_gas *parser_gas)
{
    yasm_bytecode *bc;

    switch (curtok) {
	case INSN:
	{
	    yystype insn = curval;	/* structure copy */
	    yasm_insn_operands operands;
	    int num_operands = 0;

	    get_next_token();
	    if (is_eol()) {
		/* no operands */
		return yasm_bc_create_insn(parser_gas->arch, insn.arch_data,
					   0, NULL, cur_line);
	    }

	    /* parse operands */
	    yasm_ops_initialize(&operands);
	    for (;;) {
		yasm_insn_operand *op = parse_operand(parser_gas);
		if (!op) {
		    yasm_error_set(YASM_ERROR_SYNTAX,
				   N_("expression syntax error"));
		    yasm_ops_delete(&operands, 1);
		    return NULL;
		}
		yasm_ops_append(&operands, op);
		num_operands++;

		if (is_eol())
		    break;
		if (!expect(',')) {
		    yasm_ops_delete(&operands, 1);
		    return NULL;
		}
		get_next_token();
	    }
	    return yasm_bc_create_insn(parser_gas->arch, insn.arch_data,
				       num_operands, &operands, cur_line);
	}
	case PREFIX:
	{
	    yystype prefix = curval;	/* structure copy */
	    get_next_token(); /* PREFIX */
	    bc = parse_instr(parser_gas);
	    if (!bc)
		bc = yasm_bc_create_empty_insn(parser_gas->arch, cur_line);
	    yasm_bc_insn_add_prefix(bc, prefix.arch_data);
	    return bc;
	}
	case SEGREG:
	{
	    uintptr_t segreg = SEGREG_val[0];
	    get_next_token(); /* SEGREG */
	    bc = parse_instr(parser_gas);
	    if (!bc)
		bc = yasm_bc_create_empty_insn(parser_gas->arch, cur_line);
	    yasm_bc_insn_add_seg_prefix(bc, segreg);
	}
	default:
	    return NULL;
    }
}

static int
parse_dirvals(yasm_parser_gas *parser_gas, yasm_valparamhead *vps)
{
    yasm_expr *e;
    yasm_valparam *vp;
    int num = 0;

    yasm_vps_initialize(vps);

    for (;;) {
	e = parse_expr(parser_gas);
	vp = yasm_vp_create(NULL, e);
	yasm_vps_append(vps, vp);
	num++;
	if (curtok != ',')
	    break;
	get_next_token(); /* ',' */
    }
    return num;
}

static int
parse_dirstrvals(yasm_parser_gas *parser_gas, yasm_valparamhead *vps)
{
    yasm_valparam *vp;
    int num = 0;

    yasm_vps_initialize(vps);

    for (;;) {
	if (!expect(STRING)) {
	    yasm_vps_delete(vps);
	    yasm_vps_initialize(vps);
	    return 0;
	}
	vp = yasm_vp_create(STRING_val.contents, NULL);
	yasm_vps_append(vps, vp);
	get_next_token(); /* STRING */
	num++;
	if (curtok != ',')
	    break;
	get_next_token(); /* ',' */
    }
    return num;
}

static int
parse_datavals(yasm_parser_gas *parser_gas, yasm_datavalhead *dvs)
{
    yasm_expr *e;
    yasm_dataval *dv;
    int num = 0;

    yasm_dvs_initialize(dvs);

    for (;;) {
	e = parse_expr(parser_gas);
	if (!e) {
	    yasm_dvs_delete(dvs);
	    yasm_dvs_initialize(dvs);
	    return 0;
	}
	dv = yasm_dv_create_expr(e);
	yasm_dvs_append(dvs, dv);
	num++;
	if (curtok != ',')
	    break;
	get_next_token(); /* ',' */
    }
    return num;
}

static int
parse_strvals(yasm_parser_gas *parser_gas, yasm_datavalhead *dvs)
{
    yasm_dataval *dv;
    int num = 0;

    yasm_dvs_initialize(dvs);

    for (;;) {
	if (!expect(STRING)) {
	    yasm_dvs_delete(dvs);
	    yasm_dvs_initialize(dvs);
	    return 0;
	}
	dv = yasm_dv_create_string(STRING_val.contents, STRING_val.len);
	yasm_dvs_append(dvs, dv);
	get_next_token(); /* STRING */
	num++;
	if (curtok != ',')
	    break;
	get_next_token(); /* ',' */
    }
    return num;
}

/* instruction operands */
/* memory addresses */
static yasm_effaddr *
parse_memaddr(yasm_parser_gas *parser_gas)
{
    yasm_effaddr *ea = NULL;
    yasm_expr *e1, *e2;
    int strong = 0;

    if (curtok == SEGREG) {
	uintptr_t segreg = SEGREG_val[0];
	get_next_token(); /* SEGREG */
	if (!expect(':')) return NULL;
	get_next_token(); /* ':' */
	ea = parse_memaddr(parser_gas);
	if (!ea)
	    return NULL;
	yasm_ea_set_segreg(ea, segreg);
	return ea;
    }

    /* We want to parse a leading expression, except when it's actually
     * just a memory address (with no preceding expression) such as
     * (REG...) or (,...).
     */
    get_peek_token(parser_gas);
    if (curtok != '(' || (parser_gas->peek_token != REG
			  && parser_gas->peek_token != ','))
	e1 = parse_expr(parser_gas);
    else
	e1 = NULL;

    if (curtok == '(') {
	int havereg = 0;
	uintptr_t reg;
	yasm_intnum *scale = NULL;

	get_next_token(); /* '(' */

	/* base register */
	if (curtok == REG) {
	    e2 = p_expr_new_ident(yasm_expr_reg(REG_val[0]));
	    get_next_token(); /* REG */
	} else
	    e2 = p_expr_new_ident(yasm_expr_int(yasm_intnum_create_uint(0)));

	if (curtok == ')')
	    goto done;

	if (!expect(',')) {
	    yasm_error_set(YASM_ERROR_SYNTAX, N_("invalid memory expression"));
	    if (e1) yasm_expr_destroy(e1);
	    yasm_expr_destroy(e2);
	    return NULL;
	}
	get_next_token(); /* ',' */

	if (curtok == ')')
	    goto done;

	/* index register */
	if (curtok == REG) {
	    reg = REG_val[0];
	    havereg = 1;
	    get_next_token(); /* REG */
	    if (curtok != ',') {
		scale = yasm_intnum_create_uint(1);
		goto done;
	    }
	    get_next_token(); /* ',' */
	}

	/* scale */
	if (!expect(INTNUM)) {
	    yasm_error_set(YASM_ERROR_SYNTAX, N_("non-integer scale"));
	    if (e1) yasm_expr_destroy(e1);
	    yasm_expr_destroy(e2);
	    return NULL;
	}
	scale = INTNUM_val;
	get_next_token(); /* INTNUM */

done:
	if (!expect(')')) {
	    yasm_error_set(YASM_ERROR_SYNTAX, N_("invalid memory expression"));
	    if (scale) yasm_intnum_destroy(scale);
	    if (e1) yasm_expr_destroy(e1);
	    yasm_expr_destroy(e2);
	    return NULL;
	}
	get_next_token(); /* ')' */

	if (scale) {
	    if (!havereg) {
		if (yasm_intnum_get_uint(scale) != 1)
		    yasm_warn_set(YASM_WARN_GENERAL,
			N_("scale factor of %u without an index register"),
			yasm_intnum_get_uint(scale));
		yasm_intnum_destroy(scale);
	    } else
		e2 = p_expr_new(yasm_expr_expr(e2), YASM_EXPR_ADD,
		    yasm_expr_expr(p_expr_new(yasm_expr_reg(reg), YASM_EXPR_MUL,
					      yasm_expr_int(scale))));
	}

	if (e1) {
	    /* Ordering is critical here to correctly detecting presence of
	     * RIP in RIP-relative expressions.
	     */
	    e1 = p_expr_new_tree(e2, YASM_EXPR_ADD, e1);
	} else
	    e1 = e2;
	strong = 1;
    }

    if (!e1)
	return NULL;
    ea = yasm_arch_ea_create(parser_gas->arch, e1);
    if (strong)
	yasm_ea_set_strong(ea, 1);
    return ea;
}

static yasm_insn_operand *
parse_operand(yasm_parser_gas *parser_gas)
{
    yasm_effaddr *ea;
    yasm_insn_operand *op;
    uintptr_t reg;

    switch (curtok) {
	case REG:
	    reg = REG_val[0];
	    get_next_token(); /* REG */
	    return yasm_operand_create_reg(reg);
	case SEGREG:
	    /* need to see if it's really a memory address */
	    get_peek_token(parser_gas);
	    if (parser_gas->peek_token == ':') {
		ea = parse_memaddr(parser_gas);
		if (!ea)
		    return NULL;
		return yasm_operand_create_mem(ea);
	    }
	    reg = SEGREG_val[0];
	    get_next_token(); /* SEGREG */
	    return yasm_operand_create_segreg(reg);
	case REGGROUP:
	{
	    unsigned long regindex;
	    reg = REGGROUP_val[0];
	    get_next_token(); /* REGGROUP */
	    if (curtok != '(')
		return yasm_operand_create_reg(reg);
	    get_next_token(); /* '(' */
	    if (!expect(INTNUM)) {
		yasm_error_set(YASM_ERROR_SYNTAX,
			       N_("integer register index expected"));
		return NULL;
	    }
	    regindex = yasm_intnum_get_uint(INTNUM_val);
	    get_next_token(); /* INTNUM */
	    if (!expect(')')) {
		yasm_error_set(YASM_ERROR_SYNTAX,
		    N_("missing closing parenthesis for register index"));
		return NULL;
	    }
	    get_next_token(); /* ')' */
	    reg = yasm_arch_reggroup_get_reg(parser_gas->arch, reg, regindex);
	    if (reg == 0) {
		yasm_error_set(YASM_ERROR_SYNTAX, N_("bad register index `%u'"),
			       regindex);
		return NULL;
	    }
	    return yasm_operand_create_reg(reg);
	}
	case '$':
	{
	    yasm_expr *e;
	    get_next_token(); /* '$' */
	    e = parse_expr(parser_gas);
	    if (!e) {
		yasm_error_set(YASM_ERROR_SYNTAX,
			       N_("expression missing after `%s'"), "$");
		return NULL;
	    }
	    return yasm_operand_create_imm(e);
	}
	case '*':
	    get_next_token(); /* '*' */
	    if (curtok == REG) {
		op = yasm_operand_create_reg(REG_val[0]);
		get_next_token(); /* REG */
	    } else {
		ea = parse_memaddr(parser_gas);
		if (!ea) {
		    yasm_error_set(YASM_ERROR_SYNTAX,
				   N_("expression missing after `%s'"), "*");
		    return NULL;
		}
		op = yasm_operand_create_mem(ea);
	    }
	    op->deref = 1;
	    return op;
	default:
	    ea = parse_memaddr(parser_gas);
	    if (!ea)
		return NULL;
	    return yasm_operand_create_mem(ea);
    }
}

/* Expression grammar parsed is:
 *
 * expr  : expr0 [ {+,-} expr0...]
 * expr0 : expr1 [ {|,^,&,!} expr1...]
 * expr1 : expr2 [ {*,/,%,<<,>>} expr2...]
 * expr2 : { ~,+,- } expr2
 *       | (expr)
 *       | symbol
 *       | number
 */

static yasm_expr *
parse_expr(yasm_parser_gas *parser_gas)
{
    yasm_expr *e, *f;
    e = parse_expr0(parser_gas);
    if (!e)
	return NULL;

    while (curtok == '+' || curtok == '-') {
	int op = curtok;
	get_next_token();
	f = parse_expr0(parser_gas);
	if (!f) {
	    yasm_expr_destroy(e);
	    return NULL;
	}

	switch (op) {
	    case '+': e = p_expr_new_tree(e, YASM_EXPR_ADD, f); break;
	    case '-': e = p_expr_new_tree(e, YASM_EXPR_SUB, f); break;
	}
    }
    return e;
}

static yasm_expr *
parse_expr0(yasm_parser_gas *parser_gas)
{
    yasm_expr *e, *f;
    e = parse_expr1(parser_gas);
    if (!e)
	return NULL;

    while (curtok == '|' || curtok == '^' || curtok == '&' || curtok == '!') {
	int op = curtok;
	get_next_token();
	f = parse_expr1(parser_gas);
	if (!f) {
	    yasm_expr_destroy(e);
	    return NULL;
	}

	switch (op) {
	    case '|': e = p_expr_new_tree(e, YASM_EXPR_OR, f); break;
	    case '^': e = p_expr_new_tree(e, YASM_EXPR_XOR, f); break;
	    case '&': e = p_expr_new_tree(e, YASM_EXPR_AND, f); break;
	    case '!': e = p_expr_new_tree(e, YASM_EXPR_NOR, f); break;
	}
    }
    return e;
}

static yasm_expr *
parse_expr1(yasm_parser_gas *parser_gas)
{
    yasm_expr *e, *f;
    e = parse_expr2(parser_gas);
    if (!e)
	return NULL;

    while (curtok == '*' || curtok == '/' || curtok == '%' || curtok == LEFT_OP
	   || curtok == RIGHT_OP) {
	int op = curtok;
	get_next_token();
	f = parse_expr2(parser_gas);
	if (!f) {
	    yasm_expr_destroy(e);
	    return NULL;
	}

	switch (op) {
	    case '*': e = p_expr_new_tree(e, YASM_EXPR_MUL, f); break;
	    case '/': e = p_expr_new_tree(e, YASM_EXPR_DIV, f); break;
	    case '%': e = p_expr_new_tree(e, YASM_EXPR_MOD, f); break;
	    case LEFT_OP: e = p_expr_new_tree(e, YASM_EXPR_SHL, f); break;
	    case RIGHT_OP: e = p_expr_new_tree(e, YASM_EXPR_SHR, f); break;
	}
    }
    return e;
}

static yasm_expr *
parse_expr2(yasm_parser_gas *parser_gas)
{
    yasm_expr *e;
    yasm_symrec *sym;

    switch (curtok) {
	case '+':
	    get_next_token();
	    return parse_expr2(parser_gas);
	case '-':
	    get_next_token();
	    e = parse_expr2(parser_gas);
	    if (!e)
		return NULL;
	    return p_expr_new_branch(YASM_EXPR_NEG, e);
	case '~':
	    get_next_token();
	    e = parse_expr2(parser_gas);
	    if (!e)
		return NULL;
	    return p_expr_new_branch(YASM_EXPR_NOT, e);
	case '(':
	    get_next_token();
	    e = parse_expr(parser_gas);
	    if (!e)
		return NULL;
	    if (!expect(')')) {
		yasm_error_set(YASM_ERROR_SYNTAX, N_("missing parenthesis"));
		return NULL;
	    }
	    get_next_token();
	    return e;
	case INTNUM:
	    e = p_expr_new_ident(yasm_expr_int(INTNUM_val));
	    get_next_token();
	    return e;
	case FLTNUM:
	    e = p_expr_new_ident(yasm_expr_float(FLTNUM_val));
	    get_next_token();
	    return e;
	case ID:
	case DIR_SECTNAME:
	{
	    char *name = ID_val;
	    get_next_token(); /* ID */
	    if (curtok == '@') {
		/* TODO: this is needed for shared objects, e.g. sym@PLT */
		get_next_token(); /* '@' */
		if (!expect(ID)) {
		    yasm_error_set(YASM_ERROR_SYNTAX,
				   N_("expected identifier after `@'"));
		    yasm_xfree(name);
		    return NULL;
		}
		yasm_xfree(ID_val);
		get_next_token(); /* ID */
		sym = yasm_symtab_use(p_symtab, name, cur_line);
		yasm_xfree(name);
		return p_expr_new_ident(yasm_expr_sym(sym));
	    }

	    /* "." references the current assembly position */
	    if (name[1] == '\0' && name[0] == '.')
		sym = yasm_symtab_define_curpos(p_symtab, ".",
						parser_gas->prev_bc, cur_line);
	    else
		sym = yasm_symtab_use(p_symtab, name, cur_line);
	    yasm_xfree(name);
	    return p_expr_new_ident(yasm_expr_sym(sym));
	}
	default:
	    return NULL;
    }
}

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

static void
define_lcomm(yasm_parser_gas *parser_gas, /*@only@*/ char *name,
	     yasm_expr *size, /*@null@*/ yasm_expr *align)
{
    /* Put into .bss section. */
    /*@dependent@*/ yasm_section *bss =
	gas_get_section(parser_gas, yasm__xstrdup(".bss"), NULL, NULL, NULL, 1);

    if (align) {
	/* XXX: assume alignment is in bytes, not power-of-two */
	yasm_section_bcs_append(bss, gas_parser_align(parser_gas, bss, align,
				NULL, NULL, 0));
    }

    yasm_symtab_define_label(p_symtab, name, yasm_section_bcs_last(bss), 1,
			     cur_line);
    yasm_section_bcs_append(bss, yasm_bc_create_reserve(size, 1, cur_line));
    yasm_xfree(name);
}

static yasm_section *
gas_get_section(yasm_parser_gas *parser_gas, char *name,
		/*@null@*/ char *flags, /*@null@*/ char *type,
		/*@null@*/ yasm_valparamhead *objext_valparams,
		int builtin)
{
    yasm_valparamhead vps;
    yasm_valparam *vp;
    char *gasflags;
    yasm_section *new_section;

    yasm_vps_initialize(&vps);
    vp = yasm_vp_create(name, NULL);
    yasm_vps_append(&vps, vp);

    if (!builtin) {
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
    }

    new_section = yasm_objfmt_section_switch(parser_gas->objfmt, &vps,
					     objext_valparams, cur_line);

    yasm_vps_delete(&vps);
    return new_section;
}

static void
gas_switch_section(yasm_parser_gas *parser_gas, char *name,
		   /*@null@*/ char *flags, /*@null@*/ char *type,
		   /*@null@*/ yasm_valparamhead *objext_valparams,
		   int builtin)
{
    yasm_section *new_section;

    new_section = gas_get_section(parser_gas, yasm__xstrdup(name), flags, type,
				  objext_valparams, builtin);
    if (new_section) {
	parser_gas->cur_section = new_section;
	parser_gas->prev_bc = yasm_section_bcs_last(new_section);
    } else
	yasm_error_set(YASM_ERROR_GENERAL, N_("invalid section name `%s'"),
		       name);

    yasm_xfree(name);

    if (objext_valparams)
	yasm_vps_delete(objext_valparams);
}

static yasm_bytecode *
gas_parser_align(yasm_parser_gas *parser_gas, yasm_section *sect,
		 yasm_expr *boundval, /*@null@*/ yasm_expr *fillval,
		 /*@null@*/ yasm_expr *maxskipval, int power2)
{
    yasm_intnum *boundintn;

    /* Convert power of two to number of bytes if necessary */
    if (power2)
	boundval = yasm_expr_create(YASM_EXPR_SHL,
				    yasm_expr_int(yasm_intnum_create_uint(1)),
				    yasm_expr_expr(boundval), cur_line);

    /* Largest .align in the section specifies section alignment. */
    boundintn = yasm_expr_get_intnum(&boundval, 0);
    if (boundintn) {
	unsigned long boundint = yasm_intnum_get_uint(boundintn);

	/* Alignments must be a power of two. */
	if (is_exp2(boundint)) {
	    if (boundint > yasm_section_get_align(sect))
		yasm_section_set_align(sect, boundint, cur_line);
	}
    }

    return yasm_bc_create_align(boundval, fillval, maxskipval,
				yasm_section_is_code(sect) ?
				    yasm_arch_get_fill(parser_gas->arch) : NULL,
				cur_line);
}

static yasm_bytecode *
gas_parser_dir_fill(yasm_parser_gas *parser_gas, /*@only@*/ yasm_expr *repeat,
		    /*@only@*/ /*@null@*/ yasm_expr *size,
		    /*@only@*/ /*@null@*/ yasm_expr *value)
{
    yasm_datavalhead dvs;
    yasm_bytecode *bc;
    unsigned int ssize;

    if (size) {
	/*@dependent@*/ /*@null@*/ yasm_intnum *intn;
	intn = yasm_expr_get_intnum(&size, 0);
	if (!intn) {
	    yasm_error_set(YASM_ERROR_NOT_ABSOLUTE,
			   N_("size must be an absolute expression"));
	    yasm_expr_destroy(repeat);
	    yasm_expr_destroy(size);
	    if (value)
		yasm_expr_destroy(value);
	    return NULL;
	}
	ssize = yasm_intnum_get_uint(intn);
    } else
	ssize = 1;

    if (!value)
	value = yasm_expr_create_ident(
	    yasm_expr_int(yasm_intnum_create_uint(0)), cur_line);

    yasm_dvs_initialize(&dvs);
    yasm_dvs_append(&dvs, yasm_dv_create_expr(value));
    bc = yasm_bc_create_data(&dvs, ssize, 0, parser_gas->arch, cur_line);

    yasm_bc_set_multiple(bc, repeat);

    return bc;
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
	yasm_error_set(YASM_ERROR_GENERAL, N_("unrecognized directive [%s]"),
		       name);
    }

    yasm_vps_delete(valparams);
    if (objext_valparams)
	yasm_vps_delete(objext_valparams);
}
