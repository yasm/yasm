/* $Id: parser.h,v 1.2 2001/08/19 03:52:58 peter Exp $
 * Parser module interface header file
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
#ifndef YASM_PARSER_H
#define YASM_PARSER_H

/* Interface to the parser module(s) -- the "front end" of the assembler */
typedef struct parser_s {
    /* one-line description of the parser */
    char *name;

    /* keyword used to select parser on the command line */
    char *keyword;

    /* NULL-terminated list of preprocessors that are valid to use with this
     * parser.  The raw preprocessor (raw_preproc) should always be in this
     * list so it's always possible to have no preprocessing done.
     */
    preproc **preprocs;

    /* Default preprocessor (set even if there's only one available to use) */
    preproc *default_pp;

    /* Main entrance point for the parser.
     *
     * The parser needs access to both the output format module (for format-
     * specific directives and segment names), and the selected preprocessor
     * (which should naturally be in the preprocs list above).
     *
     * This function also takes the FILE * to the initial starting file, but
     * not the filename (which is in a global variable and is not
     * parser-specific).
     *
     * This function returns the starting section of a linked list of sections
     * (whatever was in the file).
     *
     * Note that calling this has many side effects in the output format
     * module: sections and variables are declared, etc.
     */
    section *(*doparse) (preproc *pp, outfmt *of, FILE *f);
} parser;

/* Available parsers */
extern parser nasm_parser;

#endif
