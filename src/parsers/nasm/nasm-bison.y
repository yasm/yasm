/* $Id: nasm-bison.y,v 1.6 2001/05/21 22:10:27 peter Exp $
 * Main bison parser
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
%{
#include <math.h>
#include <stdlib.h>
#include "symrec.h"
#include "globals.h"
#include "bytecode.h"
#include "errwarn.h"

#define YYDEBUG 1

void init_table(void);
extern int yylex(void);
extern void yyerror(char *);

%}

%union {
    unsigned long int_val;
    char *str_val;
    double double_val;
    symrec *sym;
    effaddr ea_val;
    immval im_val;
    bytecode bc;
} 

%token <int_val> INTNUM
%token <double_val> FLTNUM
%token <str_val> DIRECTIVE_NAME DIRECTIVE_VAL
%token <int_val> BYTE WORD DWORD QWORD TWORD DQWORD
%token <int_val> DECLARE_DATA
%token <int_val> RESERVE_SPACE
%token INCBIN EQU TIMES
%token SEG WRT NEAR SHORT FAR NOSPLIT ORG
%token TO
%token O16 O32 A16 A32 LOCK REPNZ REP REPZ
%token <int_val> OPERSIZE ADDRSIZE
%token <int_val> CR4 CRREG_NOTCR4 DRREG TRREG ST0 FPUREG_NOTST0 MMXREG XMMREG
%token <int_val> REG_EAX REG_ECX REG_EDX REG_EBX REG_ESP REG_EBP REG_ESI REG_EDI
%token <int_val> REG_AX REG_CX REG_DX REG_BX REG_SP REG_BP REG_SI REG_DI
%token <int_val> REG_AL REG_CL REG_DL REG_BL REG_AH REG_CH REG_DH REG_BH
%token <int_val> REG_ES REG_CS REG_SS REG_DS REG_FS REG_GS
%token LEFT_OP RIGHT_OP SIGNDIV SIGNMOD
%token START_SECTION_OFFSET ENTRY_POINT
%token <sym> ID

/* instruction tokens (dynamically generated) */
/* @TOKENS@ */

/* @TYPES@ */

%type <bc> line exp instr instrbase

%type <int_val> fpureg reg32 reg16 reg8 reg_dess reg_fsgs reg_notcs segreg
%type <ea_val> mem memaddr memexp
%type <ea_val> mem8x mem16x mem32x mem64x mem80x mem128x
%type <ea_val> mem8 mem16 mem32 mem64 mem80 mem128 mem1632
%type <ea_val> rm8x rm16x rm32x /*rm64x xrm64x rm128x*/
%type <ea_val> rm8 rm16 rm32 rm64 rm128 xrm64
%type <im_val> immexp imm8x imm16x imm32x imm8 imm16 imm32 imm1632

%left '-' '+'
%left '*' '/'

%%
input: /* empty */
    | input line
;

line: '\n'	{ $$.len = 0; line_number++; }
    | exp '\n' { DebugPrintBC(&$1); $$ = $1; line_number++; }
    | directive '\n' { line_number++; }
    | error '\n' { yyerrok; line_number++; }
;

exp: instr
;

/* directives */
directive: '[' DIRECTIVE_NAME DIRECTIVE_VAL ']' {
	printf("Directive: Name='%s' Value='%s'\n", $2, $3);
    }
    | '[' DIRECTIVE_NAME DIRECTIVE_VAL error {
	Error(ERR_MISSING, "%c", ']');
    }
    | '[' DIRECTIVE_NAME error {
	Error(ERR_MISSING_ARG, (char *)NULL, $2);
    }
;

/* register groupings */
fpureg: ST0
    | FPUREG_NOTST0
;

reg32: REG_EAX
    | REG_ECX
    | REG_EDX
    | REG_EBX
    | REG_ESP
    | REG_EBP
    | REG_ESI
    | REG_EDI
    | DWORD reg32
;

reg16: REG_AX
    | REG_CX
    | REG_DX
    | REG_BX
    | REG_SP
    | REG_BP
    | REG_SI
    | REG_DI
    | WORD reg16
;

reg8: REG_AL
    | REG_CL
    | REG_DL
    | REG_BL
    | REG_AH
    | REG_CH
    | REG_DH
    | REG_BH
    | BYTE reg8
;

reg_dess: REG_ES
    | REG_SS
    | REG_DS
    | WORD reg_dess
;

reg_fsgs: REG_FS
    | REG_GS
    | WORD reg_fsgs
;

reg_notcs: reg_dess
    | reg_fsgs
    | WORD reg_notcs
;

segreg: reg_notcs
    | REG_CS
    | WORD segreg
;

/* memory addresses */
/* TODO: formula expansion */
memexp: INTNUM	    { (void)ConvertIntToEA(&$$, $1); }
;

memaddr: memexp			{ $$ = $1; $$.segment = 0; }
    | REG_CS ':' memaddr	{ $$ = $3; $$.segment = 0x2E; }
    | REG_SS ':' memaddr	{ $$ = $3; $$.segment = 0x36; }
    | REG_DS ':' memaddr	{ $$ = $3; $$.segment = 0x3E; }
    | REG_ES ':' memaddr	{ $$ = $3; $$.segment = 0x26; }
    | REG_FS ':' memaddr	{ $$ = $3; $$.segment = 0x64; }
    | REG_GS ':' memaddr	{ $$ = $3; $$.segment = 0x65; }
    | BYTE memaddr		{ $$ = $2; $$.addrsize = 8; $$.len = 2; }
    | WORD memaddr		{ $$ = $2; $$.addrsize = 16; $$.len = 3; }
    | DWORD memaddr		{ $$ = $2; $$.addrsize = 32; $$.len = 5; }
;

mem: '[' memaddr ']' { $$ = $2; }
;

/* explicit memory */
mem8x: BYTE mem		{ $$ = $2; }
;
mem16x: WORD mem	{ $$ = $2; }
;
mem32x: DWORD mem	{ $$ = $2; }
;
mem64x: QWORD mem	{ $$ = $2; }
;
mem80x: TWORD mem	{ $$ = $2; }
;
mem128x: DQWORD mem	{ $$ = $2; }
;

/* implicit memory */
mem8: mem
    | mem8x
;
mem16: mem
    | mem16x
;
mem32: mem
    | mem32x
;
mem64: mem
    | mem64x
;
mem80: mem
    | mem80x
;
mem128: mem
    | mem128x
;

/* both 16 and 32 bit memory */
mem1632: mem
    | mem16x
    | mem32x
;

/* explicit register or memory */
rm8x: reg8	{ (void)ConvertRegToEA(&$$, $1); }
    | mem8x
;
rm16x: reg16	{ (void)ConvertRegToEA(&$$, $1); }
    | mem16x
;
rm32x: reg32	{ (void)ConvertRegToEA(&$$, $1); }
    | mem32x
;
/* not needed:
rm64x: MMXREG	{ (void)ConvertRegToEA(&$$, $1); }
    | mem64x
;
xrm64x: XMMREG	{ (void)ConvertRegToEA(&$$, $1); }
    | mem64x
;
rm128x: XMMREG	{ (void)ConvertRegToEA(&$$, $1); }
    | mem128x
;
*/

/* implicit register or memory */
rm8: reg8	{ (void)ConvertRegToEA(&$$, $1); }
    | mem8
;
rm16: reg16	{ (void)ConvertRegToEA(&$$, $1); }
    | mem16
;
rm32: reg32	{ (void)ConvertRegToEA(&$$, $1); }
    | mem32
;
rm64: MMXREG	{ (void)ConvertRegToEA(&$$, $1); }
    | mem64
;
xrm64: XMMREG	{ (void)ConvertRegToEA(&$$, $1); }
    | mem64
;
rm128: XMMREG	{ (void)ConvertRegToEA(&$$, $1); }
    | mem128
;

/* immediate values */
/* TODO: formula expansion */
immexp: INTNUM	{ (void)ConvertIntToImm(&$$, $1); }
;

/* explicit immediates */
imm8x: BYTE immexp	{ $$ = $2; }
;
imm16x: WORD immexp	{ $$ = $2; }
;
imm32x: DWORD immexp	{ $$ = $2; }
;

/* implicit immediates */
imm8: immexp
    | imm8x
;
imm16: immexp
    | imm16x
;
imm32: immexp
    | imm32x
;

/* both 16 and 32 bit immediates */
imm1632: immexp
    | imm16x
    | imm32x
;

instr: instrbase
    | OPERSIZE instr	{ $$ = $2; $$.data.insn.opersize = $1; }
    | ADDRSIZE instr	{ $$ = $2; $$.data.insn.ea.addrsize = $1; }
    | REG_CS instr	{ $$ = $2; $$.data.insn.ea.segment = 0x2E; }
    | REG_SS instr	{ $$ = $2; $$.data.insn.ea.segment = 0x36; }
    | REG_DS instr	{ $$ = $2; $$.data.insn.ea.segment = 0x3E; }
    | REG_ES instr	{ $$ = $2; $$.data.insn.ea.segment = 0x26; }
    | REG_FS instr	{ $$ = $2; $$.data.insn.ea.segment = 0x64; }
    | REG_GS instr	{ $$ = $2; $$.data.insn.ea.segment = 0x65; }
    | LOCK instr	{ $$ = $2; $$.data.insn.lockrep_pre = 0xF0; }
    | REPNZ instr	{ $$ = $2; $$.data.insn.lockrep_pre = 0xF2; }
    | REP instr		{ $$ = $2; $$.data.insn.lockrep_pre = 0xF3; }
    | REPZ instr	{ $$ = $2; $$.data.insn.lockrep_pre = 0xF4; }
;

/* instruction grammars (dynamically generated) */
/* @INSTRUCTIONS@ */

