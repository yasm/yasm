/* $IdPath$
 * NASM-compatible parser header file
 *
 *  Copyright (C) 2002  Peter Johnson
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
#ifndef YASM_NASM_PARSER_H
#define YASM_NASM_PARSER_H

#include "nasm-bison.h"

#define YYCTYPE		char
typedef struct Scanner {
    YYCTYPE		*bot, *tok, *ptr, *cur, *pos, *lim, *top, *eof;
    unsigned int	tchar, tline, cline;
} Scanner;

#define MAX_SAVED_LINE_LEN  80

typedef struct yasm_parser_nasm {
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
    YYCTYPE save_line[MAX_SAVED_LINE_LEN];

    Scanner s;
    enum {
	INITIAL,
	DIRECTIVE,
	DIRECTIVE2,
	LINECHG,
	LINECHG2
    } state;
} yasm_parser_nasm;

/* shorter access names to commonly used parser_nasm fields */
#define p_symtab	(parser_nasm->symtab)

#define cur_line	(yasm_linemap_get_current(parser_nasm->linemap))

#define p_expr_new_tree(l,o,r)	yasm_expr_create_tree(l,o,r,cur_line)
#define p_expr_new_branch(o,r)	yasm_expr_create_branch(o,r,cur_line)
#define p_expr_new_ident(r)	yasm_expr_create_ident(r,cur_line)

int nasm_parser_parse(void *parser_nasm_arg);
void nasm_parser_cleanup(yasm_parser_nasm *parser_nasm);
int nasm_parser_lex(YYSTYPE *lvalp, yasm_parser_nasm *parser_nasm);

#endif
