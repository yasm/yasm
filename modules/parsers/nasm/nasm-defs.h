/* $IdPath$
 * Variable name redefinitions for NASM-compatible parser
 *
 *  Copyright (C) 2001  Peter Johnson
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
#define yy_create_buffer	nasm_parser__create_buffer
#define yy_delete_buffer	nasm_parser__delete_buffer
#define yy_init_buffer		nasm_parser__init_buffer
#define yy_load_buffer_state	nasm_parser__load_buffer_state
#define yy_switch_to_buffer	nasm_parser__switch_to_buffer
#define yyact			nasm_parser_act
#define yyback			nasm_parser_back
#define yybgin			nasm_parser_bgin
#define yychar			nasm_parser_char
#define yychk			nasm_parser_chk
#define yycrank			nasm_parser_crank
#define yydebug			nasm_parser_debug
#define yydef			nasm_parser_def
#define yyerrflag		nasm_parser_errflag
#define yyerror			nasm_parser_error
#define yyestate		nasm_parser_estate
#define yyexca			nasm_parser_exca
#define yyextra			nasm_parser_extra
#define yyfnd			nasm_parser_fnd
#define yyin			nasm_parser_in
#define yyinput			nasm_parser_input
#define yyleng			nasm_parser_leng
#define yylex			nasm_parser_lex
#define yylineno		nasm_parser_lineno
#define yylook			nasm_parser_look
#define yylsp			nasm_parser_lsp
#define yylstate		nasm_parser_lstate
#define yylval			nasm_parser_lval
#define yymatch			nasm_parser_match
#define yymorfg			nasm_parser_morfg
#define yynerrs			nasm_parser_nerrs
#define yyolsp			nasm_parser_olsp
#define yyout			nasm_parser_out
#define yyoutput		nasm_parser_output
#define yypact			nasm_parser_pact
#define yyparse			nasm_parser_parse
#define yypgo			nasm_parser_pgo
#define yyprevious		nasm_parser_previous
#define yyps			nasm_parser_ps
#define yypv			nasm_parser_pv
#define yyr1			nasm_parser_r1
#define yyr2			nasm_parser_r2
#define yyreds			nasm_parser_reds
#define yyrestart		nasm_parser_restart
#define yys			nasm_parser_s
#define yysbuf			nasm_parser_sbuf
#define yysptr			nasm_parser_sptr
#define yystate			nasm_parser_state
#define yysvec			nasm_parser_svec
#define yytchar			nasm_parser_tchar
#define yytext			nasm_parser_text
#define yytmp			nasm_parser_tmp
#define yytoks			nasm_parser_toks
#define yytop			nasm_parser_top
#define yyunput			nasm_parser_unput
#define yyv			nasm_parser_v
#define yyval			nasm_parser_val
#define yyvstop			nasm_parser_vstop
/*#define yywrap			nasm_parser_wrap*/
