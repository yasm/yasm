/* $Id$
 * GAS-compatible parser header file
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
#ifndef YASM_NASM_PARSER_H
#define YASM_NASM_PARSER_H

#include "gas-bison.h"

#define YYCTYPE		char
typedef struct Scanner {
    YYCTYPE		*bot, *tok, *ptr, *cur, *pos, *lim, *top, *eof;
    unsigned int	tchar, tline, cline;
} Scanner;

#define MAX_SAVED_LINE_LEN  80

typedef struct gas_rept_line {
    STAILQ_ENTRY(gas_rept_line) link;
    YYCTYPE *data;		/* line characters */
    size_t len;			/* length of data */
} gas_rept_line;

typedef struct gas_rept {
    STAILQ_HEAD(reptlinelist, gas_rept_line) lines;	/* repeated lines */
    unsigned long startline;	/* line number of rept directive */
    unsigned long numrept;	/* number of repititions to generate */
    unsigned long numdone;	/* number of repititions executed so far */
    /*@null@*/ gas_rept_line *line;	/* next line to repeat */
    size_t linepos;		/* position to start pulling chars from line */
    int ended;			/* seen endr directive yet? */

    char *oldbuf;		/* saved previous fill buffer */
    size_t oldbuflen;		/* previous fill buffer length */
    size_t oldbufpos;		/* position in previous fill buffer */
} gas_rept;

typedef struct yasm_parser_gas {
    FILE *in;
    int debug;

    /*@only@*/ yasm_object *object;
    /*@dependent@*/ yasm_section *cur_section;

    /* last "base" label for local (.) labels */
    /*@null@*/ char *locallabel_base;
    size_t locallabel_base_len;

    /*@dependent@*/ yasm_preproc *preproc;
    /*@dependent@*/ yasm_arch *arch;
    /*@dependent@*/ yasm_objfmt *objfmt;

    /*@dependent@*/ yasm_linemap *linemap;
    /*@dependent@*/ yasm_symtab *symtab;

    /*@null@*/ yasm_bytecode *prev_bc;
    yasm_bytecode *temp_bc;

    int save_input;
    YYCTYPE save_line[2][MAX_SAVED_LINE_LEN];
    int save_last;

    Scanner s;
    enum {
	INITIAL,
	SECTION_DIRECTIVE
    } state;

    int code_section;

    /*@null@*/ gas_rept *rept;
} yasm_parser_gas;

/* shorter access names to commonly used parser_gas fields */
#define p_symtab	(parser_gas->symtab)

#define cur_line	(yasm_linemap_get_current(parser_gas->linemap))

#define p_expr_new(l,o,r)	yasm_expr_create(o,l,r,cur_line)
#define p_expr_new_tree(l,o,r)	yasm_expr_create_tree(l,o,r,cur_line)
#define p_expr_new_branch(o,r)	yasm_expr_create_branch(o,r,cur_line)
#define p_expr_new_ident(r)	yasm_expr_create_ident(r,cur_line)

int gas_parser_parse(void *parser_gas_arg);
void gas_parser_cleanup(yasm_parser_gas *parser_gas);
int gas_parser_lex(YYSTYPE *lvalp, yasm_parser_gas *parser_gas);

#endif
