/* $IdPath$
 * NASM-compatible parser header file
 *
 *  Copyright (C) 2002  Peter Johnson
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
#ifndef YASM_NASM_PARSER_H
#define YASM_NASM_PARSER_H

int nasm_parser_parse(void);
void nasm_parser_cleanup(void);
int nasm_parser_lex(void);
void nasm_parser_set_directive_state(void);

extern FILE *nasm_parser_in;
extern int nasm_parser_debug;
extern size_t (*nasm_parser_input) (char *buf, size_t max_size);

extern sectionhead nasm_parser_sections;
extern /*@dependent@*/ section *nasm_parser_cur_section;

extern char *nasm_parser_locallabel_base;
extern size_t nasm_parser_locallabel_base_len;

extern /*@dependent@*/ arch *nasm_parser_arch;
extern /*@dependent@*/ objfmt *nasm_parser_objfmt;
extern /*@dependent@*/ linemgr *nasm_parser_linemgr;
extern /*@dependent@*/ errwarn *nasm_parser_errwarn;

#define cur_lindex	(nasm_parser_linemgr->get_current())

#define p_expr_new_tree(l,o,r)	expr_new_tree(l,o,r,cur_lindex)
#define p_expr_new_branch(o,r)	expr_new_branch(o,r,cur_lindex)
#define p_expr_new_ident(r)	expr_new_ident(r,cur_lindex)

#define cur_we		nasm_parser_errwarn

#endif
