/* $IdPath$
 * YAPP preprocessor (mimics NASM's preprocessor) header file
 *
 *   Copyright (C) 2001  Michael Urman
 *
 *   This file is part of YASM.
 *
 *   YASM is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   YASM is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
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

/* Build source and macro representations */
static SLIST_HEAD(source_head, source_s) source_head, macro_head, param_head, *current_head;
static struct source_s {
    SLIST_ENTRY(source_s) next;
    YAPP_Token token;
} *src, *source_tail, *macro_tail, *param_tail, **current_tail;
typedef struct source_s source;

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
    YAPP_OUTPUT,	/* this level+module outputs */
    YAPP_NO_OUTPUT,	/* this would never output */
    YAPP_OLD_OUTPUT,	/* this level has already output */
    YAPP_BLOCKED_OUTPUT /* the surrounding level is not outputting */
} YAPP_Output;

/* don't forget what the nesting level says */
static SLIST_HEAD(output_head, output_s) output_head;
static struct output_s {
    SLIST_ENTRY(output_s) next;
    YAPP_Output out;
} output, *out;
