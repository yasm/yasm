/* $IdPath: yasm/modules/parsers/nasm/nasm-parser.h,v 1.6 2003/03/13 06:54:19 peter Exp $
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

int nasm_parser_parse(void);
void nasm_parser_cleanup(void);
int nasm_parser_lex(void);
void nasm_parser_set_directive_state(void);

extern FILE *nasm_parser_in;
extern int nasm_parser_debug;
extern size_t (*nasm_parser_input) (char *buf, size_t max_size);

extern yasm_sectionhead *nasm_parser_sections;
extern /*@dependent@*/ yasm_section *nasm_parser_cur_section;

extern char *nasm_parser_locallabel_base;
extern size_t nasm_parser_locallabel_base_len;

extern /*@dependent@*/ yasm_arch *nasm_parser_arch;
extern /*@dependent@*/ yasm_objfmt *nasm_parser_objfmt;
extern /*@dependent@*/ yasm_linemgr *nasm_parser_linemgr;

extern int nasm_parser_save_input;

#define cur_lindex	(nasm_parser_linemgr->get_current())

#define p_expr_new_tree(l,o,r)	yasm_expr_new_tree(l,o,r,cur_lindex)
#define p_expr_new_branch(o,r)	yasm_expr_new_branch(o,r,cur_lindex)
#define p_expr_new_ident(r)	yasm_expr_new_ident(r,cur_lindex)

#endif
