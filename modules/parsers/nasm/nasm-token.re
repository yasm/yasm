/*
 * NASM-compatible re2c lexer
 *
 *  Copyright (C) 2001-2007  Peter Johnson
 *
 *  Portions based on re2c's example code.
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
#include <util.h>
RCSID("$Id$");

#include <libyasm.h>

#include "modules/parsers/nasm/nasm-parser.h"


#define YYCURSOR        cursor
#define YYLIMIT         (s->lim)
#define YYMARKER        (s->ptr)
#define YYFILL(n)       {}

#define RETURN(i)       {s->cur = cursor; parser_nasm->tokch = s->tok[0]; \
                         return i;}

#define SCANINIT()      {s->tok = cursor;}

#define TOK             ((char *)s->tok)
#define TOKLEN          (size_t)(cursor-s->tok)


/* starting size of string buffer */
#define STRBUF_ALLOC_SIZE       128

/* string buffer used when parsing strings/character constants */
static YYCTYPE *strbuf = NULL;

/* length of strbuf (including terminating NULL character) */
static size_t strbuf_size = 0;

static int linechg_numcount;

/*!re2c
  any = [\001-\377];
  digit = [0-9];
  iletter = [a-zA-Z];
  bindigit = [01];
  octdigit = [0-7];
  hexdigit = [0-9a-fA-F];
  ws = [ \t\r];
  quot = ["'];
*/


int
nasm_parser_lex(YYSTYPE *lvalp, yasm_parser_nasm *parser_nasm)
{
    yasm_scanner *s = &parser_nasm->s;
    YYCTYPE *cursor = s->cur;
    YYCTYPE endch;
    size_t count, len;
    YYCTYPE savech;

    /* Handle one token of lookahead */
    if (parser_nasm->peek_token != NONE) {
        int tok = parser_nasm->peek_token;
        *lvalp = parser_nasm->peek_tokval;  /* structure copy */
        parser_nasm->tokch = parser_nasm->peek_tokch;
        parser_nasm->peek_token = NONE;
        return tok;
    }

    /* Catch EOL (EOF from the scanner perspective) */
    if (s->eof && cursor == s->eof)
        return 0;

    /* Jump to proper "exclusive" states */
    switch (parser_nasm->state) {
        case DIRECTIVE:
            goto directive;
        case SECTION_DIRECTIVE:
            goto section_directive;
        case DIRECTIVE2:
            goto directive2;
        case LINECHG:
            goto linechg;
        case LINECHG2:
            goto linechg2;
        default:
            break;
    }

scan:
    SCANINIT();

    /*!re2c
        /* standard decimal integer */
        digit+ {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            lvalp->intn = yasm_intnum_create_dec(TOK);
            s->tok[TOKLEN] = savech;
            RETURN(INTNUM);
        }
        /* 10010011b - binary number */

        bindigit+ 'b' {
            s->tok[TOKLEN-1] = '\0'; /* strip off 'b' */
            lvalp->intn = yasm_intnum_create_bin(TOK);
            RETURN(INTNUM);
        }

        /* 777q or 777o - octal number */
        octdigit+ [qQoO] {
            s->tok[TOKLEN-1] = '\0'; /* strip off 'q' or 'o' */
            lvalp->intn = yasm_intnum_create_oct(TOK);
            RETURN(INTNUM);
        }

        /* 0AAh form of hexidecimal number */
        digit hexdigit* 'h' {
            s->tok[TOKLEN-1] = '\0'; /* strip off 'h' */
            lvalp->intn = yasm_intnum_create_hex(TOK);
            RETURN(INTNUM);
        }

        /* $0AA and 0xAA forms of hexidecimal number */
        (("$" digit) | '0x') hexdigit+ {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            if (s->tok[1] == 'x' || s->tok[1] == 'X')
                /* skip 0 and x */
                lvalp->intn = yasm_intnum_create_hex(TOK+2);
            else
                /* don't skip 0 */
                lvalp->intn = yasm_intnum_create_hex(TOK+1);
            s->tok[TOKLEN] = savech;
            RETURN(INTNUM);
        }

        /* floating point value */
        digit+ "." digit* ('e' [-+]? digit+)? {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            lvalp->flt = yasm_floatnum_create(TOK);
            s->tok[TOKLEN] = savech;
            RETURN(FLTNUM);
        }

        /* string/character constant values */
        quot {
            endch = s->tok[0];
            goto stringconst;
        }

        /* %line linenum+lineinc filename */
        "%line" {
            parser_nasm->state = LINECHG;
            linechg_numcount = 0;
            RETURN(LINE);
        }

        /* size specifiers */
        'byte'          { lvalp->int_info = 8; RETURN(SIZE_OVERRIDE); }
        'hword'         {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)/2;
            RETURN(SIZE_OVERRIDE);
        }
        'word'          {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch);
            RETURN(SIZE_OVERRIDE);
        }
        'dword' | 'long'        {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*2;
            RETURN(SIZE_OVERRIDE);
        }
        'qword'         {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*4;
            RETURN(SIZE_OVERRIDE);
        }
        'tword'         { lvalp->int_info = 80; RETURN(SIZE_OVERRIDE); }
        'dqword'        {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*8;
            RETURN(SIZE_OVERRIDE);
        }
        'oword'        {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*8;
            RETURN(SIZE_OVERRIDE);
        }

        /* pseudo-instructions */
        'db'            { lvalp->int_info = 8; RETURN(DECLARE_DATA); }
        'dhw'           {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)/2;
            RETURN(DECLARE_DATA);
        }
        'dw'            {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch);
            RETURN(DECLARE_DATA);
        }
        'dd'            {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*2;
            RETURN(DECLARE_DATA);
        }
        'dq'            {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*4;
            RETURN(DECLARE_DATA);
        }
        'dt'            { lvalp->int_info = 80; RETURN(DECLARE_DATA); }
        'ddq'           {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*8;
            RETURN(DECLARE_DATA);
        }
        'do'           {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*8;
            RETURN(DECLARE_DATA);
        }

        'resb'          { lvalp->int_info = 8; RETURN(RESERVE_SPACE); }
        'reshw'         {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)/2;
            RETURN(RESERVE_SPACE);
        }
        'resw'          {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch);
            RETURN(RESERVE_SPACE);
        }
        'resd'          {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*2;
            RETURN(RESERVE_SPACE);
        }
        'resq'          {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*4;
            RETURN(RESERVE_SPACE);
        }
        'rest'          { lvalp->int_info = 80; RETURN(RESERVE_SPACE); }
        'resdq'         {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*8;
            RETURN(RESERVE_SPACE);
        }
        'reso'         {
            lvalp->int_info = yasm_arch_wordsize(p_object->arch)*8;
            RETURN(RESERVE_SPACE);
        }

        'incbin'        { RETURN(INCBIN); }

        'equ'           { RETURN(EQU); }

        'times'         { RETURN(TIMES); }

        'seg'           { RETURN(SEG); }
        'wrt'           { RETURN(WRT); }

        'abs'           { RETURN(ABS); }
        'rel'           { RETURN(REL); }

        'nosplit'       { RETURN(NOSPLIT); }
        'strict'        { RETURN(STRICT); }

        /* operators */
        "<<"                    { RETURN(LEFT_OP); }
        ">>"                    { RETURN(RIGHT_OP); }
        "//"                    { RETURN(SIGNDIV); }
        "%%"                    { RETURN(SIGNMOD); }
        "$$"                    { RETURN(START_SECTION_ID); }
        [-+|^*&/%~$():=,\[]     { RETURN(s->tok[0]); }
        "]"                     { RETURN(s->tok[0]); }

        /* special non-local ..@label and labels like ..start */
        ".." [a-zA-Z0-9_$#@~.?]+ {
            lvalp->str_val = yasm__xstrndup(TOK, TOKLEN);
            RETURN(SPECIAL_ID);
        }

        /* local label (.label) */
        "." [a-zA-Z0-9_$#@~?][a-zA-Z0-9_$#@~.?]* {
            if (!parser_nasm->locallabel_base) {
                lvalp->str_val = yasm__xstrndup(TOK, TOKLEN);
                yasm_warn_set(YASM_WARN_GENERAL,
                              N_("no non-local label before `%s'"),
                              lvalp->str_val);
            } else {
                len = TOKLEN + parser_nasm->locallabel_base_len;
                lvalp->str_val = yasm_xmalloc(len + 1);
                strcpy(lvalp->str_val, parser_nasm->locallabel_base);
                strncat(lvalp->str_val, TOK, TOKLEN);
                lvalp->str_val[len] = '\0';
            }

            RETURN(LOCAL_ID);
        }

        /* forced identifier */
        "$" [a-zA-Z0-9_$#@~.?]+ {
            lvalp->str_val = yasm__xstrndup(TOK+1, TOKLEN-1);
            RETURN(ID);
        }

        /* identifier that may be a register, instruction, etc. */
        [a-zA-Z_?][a-zA-Z0-9_$#@~.?]* {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            if (parser_nasm->state != INSTRUCTION) {
                uintptr_t prefix;
                switch (yasm_arch_parse_check_insnprefix
                        (p_object->arch, TOK, TOKLEN, cur_line, &lvalp->bc,
                         &prefix)) {
                    case YASM_ARCH_INSN:
                        parser_nasm->state = INSTRUCTION;
                        s->tok[TOKLEN] = savech;
                        RETURN(INSN);
                    case YASM_ARCH_PREFIX:
                        lvalp->arch_data = prefix;
                        s->tok[TOKLEN] = savech;
                        RETURN(PREFIX);
                    default:
                        break;
                }
            }
            switch (yasm_arch_parse_check_regtmod
                    (p_object->arch, TOK, TOKLEN, &lvalp->arch_data)) {
                case YASM_ARCH_REG:
                    s->tok[TOKLEN] = savech;
                    RETURN(REG);
                case YASM_ARCH_SEGREG:
                    s->tok[TOKLEN] = savech;
                    RETURN(SEGREG);
                case YASM_ARCH_TARGETMOD:
                    s->tok[TOKLEN] = savech;
                    RETURN(TARGETMOD);
                default:
                    s->tok[TOKLEN] = savech;
            }
            /* Propagate errors in case we got a warning from the arch */
            yasm_errwarn_propagate(parser_nasm->errwarns, cur_line);
            /* Just an identifier, return as such. */
            lvalp->str_val = yasm__xstrndup(TOK, TOKLEN);
            RETURN(ID);
        }

        ";" (any \ [\n])*       { goto scan; }

        ws+                     { goto scan; }

        [\000]                  {
            parser_nasm->state = INITIAL;
            RETURN(s->tok[0]);
        }

        any {
            yasm_warn_set(YASM_WARN_UNREC_CHAR,
                          N_("ignoring unrecognized character `%s'"),
                          yasm__conv_unprint(s->tok[0]));
            goto scan;
        }
    */

    /* %line linenum+lineinc filename */
linechg:
    SCANINIT();

    /*!re2c
        digit+ {
            linechg_numcount++;
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            lvalp->intn = yasm_intnum_create_dec(TOK);
            s->tok[TOKLEN] = savech;
            RETURN(INTNUM);
        }

        [\000] {
            parser_nasm->state = INITIAL;
            RETURN(s->tok[0]);
        }

        "+" {
            RETURN(s->tok[0]);
        }

        ws+ {
            if (linechg_numcount == 2) {
                parser_nasm->state = LINECHG2;
                goto linechg2;
            }
            goto linechg;
        }

        any {
            yasm_warn_set(YASM_WARN_UNREC_CHAR,
                          N_("ignoring unrecognized character `%s'"),
                          yasm__conv_unprint(s->tok[0]));
            goto linechg;
        }
    */

linechg2:
    SCANINIT();

    /*!re2c
        [\000] {
            parser_nasm->state = INITIAL;
            RETURN(s->tok[0]);
        }

        "\r" { }

        (any \ [\r\n])+ {
            parser_nasm->state = LINECHG;
            lvalp->str_val = yasm__xstrndup(TOK, TOKLEN);
            RETURN(FILENAME);
        }
    */

    /* directive: [name value] */
directive:
    SCANINIT();

    /*!re2c
        [\]\000] {
            parser_nasm->state = INITIAL;
            RETURN(s->tok[0]);
        }

        [a-zA-Z_][a-zA-Z_0-9]* {
            lvalp->str_val = yasm__xstrndup(TOK, TOKLEN);
            if (yasm__strcasecmp(lvalp->str_val, "section") == 0 ||
                yasm__strcasecmp(lvalp->str_val, "segment") == 0)
                parser_nasm->state = SECTION_DIRECTIVE;
            else
                parser_nasm->state = DIRECTIVE2;
            RETURN(DIRECTIVE_NAME);
        }

        any {
            yasm_warn_set(YASM_WARN_UNREC_CHAR,
                          N_("ignoring unrecognized character `%s'"),
                          yasm__conv_unprint(s->tok[0]));
            goto directive;
        }
    */

    /* section directive (the section name portion thereof) */
section_directive:
    SCANINIT();

    /*!re2c
        [a-zA-Z0-9_$#@~.?-]+ {
            lvalp->str.contents = yasm__xstrndup(TOK, TOKLEN);
            lvalp->str.len = TOKLEN;
            parser_nasm->state = DIRECTIVE2;
            RETURN(STRING);
        }

        quot            {
            parser_nasm->state = DIRECTIVE2;
            endch = s->tok[0];
            goto stringconst;
        }

        ws+             {
            parser_nasm->state = DIRECTIVE2;
            goto section_directive;
        }

        "]" {
            parser_nasm->state = INITIAL;
            RETURN(s->tok[0]);
        }

        [\000]          {
            parser_nasm->state = INITIAL;
            RETURN(s->tok[0]);
        }

        any {
            yasm_warn_set(YASM_WARN_UNREC_CHAR,
                          N_("ignoring unrecognized character `%s'"),
                          yasm__conv_unprint(s->tok[0]));
            goto section_directive;
        }
    */

    /* inner part of directive */
directive2:
    SCANINIT();

    /*!re2c
        /* standard decimal integer */
        digit+ {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            lvalp->intn = yasm_intnum_create_dec(TOK);
            s->tok[TOKLEN] = savech;
            RETURN(INTNUM);
        }
        /* 10010011b - binary number */

        bindigit+ 'b' {
            s->tok[TOKLEN-1] = '\0'; /* strip off 'b' */
            lvalp->intn = yasm_intnum_create_bin(TOK);
            RETURN(INTNUM);
        }

        /* 777q or 777o - octal number */
        octdigit+ [qQoO] {
            s->tok[TOKLEN-1] = '\0'; /* strip off 'q' or 'o' */
            lvalp->intn = yasm_intnum_create_oct(TOK);
            RETURN(INTNUM);
        }

        /* 0AAh form of hexidecimal number */
        digit hexdigit* 'h' {
            s->tok[TOKLEN-1] = '\0'; /* strip off 'h' */
            lvalp->intn = yasm_intnum_create_hex(TOK);
            RETURN(INTNUM);
        }

        /* $0AA and 0xAA forms of hexidecimal number */
        (("$" digit) | "0x") hexdigit+ {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            if (s->tok[1] == 'x')
                /* skip 0 and x */
                lvalp->intn = yasm_intnum_create_hex(TOK+2);
            else
                /* don't skip 0 */
                lvalp->intn = yasm_intnum_create_hex(TOK+1);
            s->tok[TOKLEN] = savech;
            RETURN(INTNUM);
        }

        /* string/character constant values */
        quot {
            endch = s->tok[0];
            goto stringconst;
        }

        /* operators */
        "<<"                    { RETURN(LEFT_OP); }
        ">>"                    { RETURN(RIGHT_OP); }
        "//"                    { RETURN(SIGNDIV); }
        "%%"                    { RETURN(SIGNMOD); }
        [-+|^*&/%~$():=,\[]     { RETURN(s->tok[0]); }

        /* handle ] for directives */
        "]" {
            parser_nasm->state = INITIAL;
            RETURN(s->tok[0]);
        }

        /* forced identifier; within directive, don't strip '$', this is
         * handled later.
         */
        "$" [a-zA-Z0-9_$#@~.?]+ {
            lvalp->str_val = yasm__xstrndup(TOK, TOKLEN);
            RETURN(ID);
        }

        /* identifier; within directive, no local label mechanism */
        [a-zA-Z_.?][a-zA-Z0-9_$#@~.?]* {
            savech = s->tok[TOKLEN];
            s->tok[TOKLEN] = '\0';
            switch (yasm_arch_parse_check_regtmod
                    (p_object->arch, TOK, TOKLEN, &lvalp->arch_data)) {
                case YASM_ARCH_REG:
                    s->tok[TOKLEN] = savech;
                    RETURN(REG);
                default:
                    s->tok[TOKLEN] = savech;
            }
            /* Propagate errors in case we got a warning from the arch */
            yasm_errwarn_propagate(parser_nasm->errwarns, cur_line);
            /* Just an identifier, return as such. */
            lvalp->str_val = yasm__xstrndup(TOK, TOKLEN);
            RETURN(ID);
        }

        ";" (any \ [\n])*       { goto directive2; }

        ws+                     { goto directive2; }

        [\000]                  {
            parser_nasm->state = INITIAL;
            RETURN(s->tok[0]);
        }

        any {
            yasm_warn_set(YASM_WARN_UNREC_CHAR,
                          N_("ignoring unrecognized character `%s'"),
                          yasm__conv_unprint(s->tok[0]));
            goto scan;
        }
     */

    /* string/character constant values */
stringconst:
    strbuf = yasm_xmalloc(STRBUF_ALLOC_SIZE);
    strbuf_size = STRBUF_ALLOC_SIZE;
    count = 0;

stringconst_scan:
    SCANINIT();

    /*!re2c
        [\000]  {
            yasm_error_set(YASM_ERROR_SYNTAX, N_("unterminated string"));
            strbuf[count] = '\0';
            lvalp->str.contents = (char *)strbuf;
            lvalp->str.len = count;
            RETURN(STRING);
        }

        any     {
            if (s->tok[0] == endch) {
                strbuf[count] = '\0';
                lvalp->str.contents = (char *)strbuf;
                lvalp->str.len = count;
                RETURN(STRING);
            }

            strbuf[count++] = s->tok[0];
            if (count >= strbuf_size) {
                strbuf = yasm_xrealloc(strbuf, strbuf_size + STRBUF_ALLOC_SIZE);
                strbuf_size += STRBUF_ALLOC_SIZE;
            }

            goto stringconst_scan;
        }
    */
}
