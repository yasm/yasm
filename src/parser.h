/* $IdPath$
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
struct parser {
    /* one-line description of the parser */
    const char *name;

    /* keyword used to select parser on the command line */
    const char *keyword;

    /* NULL-terminated list of preprocessors that are valid to use with this
     * parser.  The raw preprocessor (raw_preproc) should always be in this
     * list so it's always possible to have no preprocessing done.
     */
    const char **preproc_keywords;

    /* Default preprocessor. */
    const char *default_preproc_keyword;

    /* Main entrance point for the parser.
     *
     * The parser needs access to both the object format module (for format-
     * specific directives and segment names), and the preprocessor.
     *
     * This function also takes the FILE * to the initial starting file and
     * the input filename.
     *
     * save_input is nonzero if the parser needs to save the original lines of
     * source in the input file into the linemgr via linemgr->add_assoc_data().
     *
     * This function returns the starting section of a linked list of sections
     * (whatever was in the file).
     */
    sectionhead *(*do_parse) (preproc *pp, arch *a, objfmt *of, linemgr *lm,
			      errwarn *we, FILE *f, const char *in_filename,
			      int save_input);
};

/* Generic functions for all parsers - implemented in src/parser.c */

/* Lists all available parsers.  Calls printfunc with the name and keyword
 * of each available parser.
 */
void list_parsers(void (*printfunc) (const char *name, const char *keyword));

#endif
