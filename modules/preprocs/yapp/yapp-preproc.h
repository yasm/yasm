/*
 * YAPP preprocessor (mimics NASM's preprocessor) header file
 *
 * Copyright (C) 2001  Michael Urman
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

/* Representation of tokenized file, both for straight source, and macros */
typedef struct YAPP_Token_s {
    unsigned int type;
    char *str;
    union {
        unsigned int int_val;
        double double_val;
        char *str_val;
    } val;
} YAPP_Token;

/* internal state of preprocessor's parser */
typedef enum {
    YAPP_STATE_INITIAL = 0,
    YAPP_STATE_ASSIGN,
    YAPP_STATE_DEFINING_MACRO,
    YAPP_STATE_BUILDING_MACRO,
    YAPP_STATE_NEED_EOL,
    YAPP_STATE_EOF
} YAPP_State;

/* tracks nested %if* %elif* %else %endif structures */
typedef enum {
    YAPP_OUTPUT,        /* this level+module outputs */
    YAPP_NO_OUTPUT,     /* this would never output */
    YAPP_OLD_OUTPUT,    /* this level has already output */
    YAPP_BLOCKED_OUTPUT /* the surrounding level is not outputting */
} YAPP_Output;

void yapp_lex_initialize(FILE *f);
void set_inhibit(void);

extern /*@dependent@*/ yasm_linemap *yapp_preproc_linemap;
#define cur_lindex      yasm_linemap_get_current(yapp_preproc_linemap)

