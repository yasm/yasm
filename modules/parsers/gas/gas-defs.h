/* $Id$
 * Variable name redefinitions for GAS-compatible parser
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
#define yy_create_buffer	gas_parser__create_buffer
#define yy_delete_buffer	gas_parser__delete_buffer
#define yy_init_buffer		gas_parser__init_buffer
#define yy_load_buffer_state	gas_parser__load_buffer_state
#define yy_switch_to_buffer	gas_parser__switch_to_buffer
#define yyact			gas_parser_act
#define yyback			gas_parser_back
#define yybgin			gas_parser_bgin
#define yychar			gas_parser_char
#define yychk			gas_parser_chk
#define yycrank			gas_parser_crank
#define yydebug			gas_parser_debug
#define yydef			gas_parser_def
#define yyerrflag		gas_parser_errflag
#define yyerror			gas_parser_error
#define yyestate		gas_parser_estate
#define yyexca			gas_parser_exca
#define yyextra			gas_parser_extra
#define yyfnd			gas_parser_fnd
#define yyin			gas_parser_in
#define yyinput			gas_parser_input
#define yyleng			gas_parser_leng
#define yylex			gas_parser_lex
#define yylineno		gas_parser_lineno
#define yylook			gas_parser_look
#define yylsp			gas_parser_lsp
#define yylstate		gas_parser_lstate
#define yylval			gas_parser_lval
#define yymatch			gas_parser_match
#define yymorfg			gas_parser_morfg
#define yynerrs			gas_parser_nerrs
#define yyolsp			gas_parser_olsp
#define yyout			gas_parser_out
#define yyoutput		gas_parser_output
#define yypact			gas_parser_pact
#define yyparse			gas_parser_parse
#define yypgo			gas_parser_pgo
#define yyprevious		gas_parser_previous
#define yyps			gas_parser_ps
#define yypv			gas_parser_pv
#define yyr1			gas_parser_r1
#define yyr2			gas_parser_r2
#define yyreds			gas_parser_reds
#define yyrestart		gas_parser_restart
#define yys			gas_parser_s
#define yysbuf			gas_parser_sbuf
#define yysptr			gas_parser_sptr
#define yystate			gas_parser_state
#define yysvec			gas_parser_svec
#define yytchar			gas_parser_tchar
#define yytext			gas_parser_text
#define yytmp			gas_parser_tmp
#define yytoks			gas_parser_toks
#define yytop			gas_parser_top
#define yyunput			gas_parser_unput
#define yyv			gas_parser_v
#define yyval			gas_parser_val
#define yyvstop			gas_parser_vstop
/*#define yywrap			gas_parser_wrap*/
