/* $IdPath$
 * Parser module interface header file
 *
 *  Copyright (C) 2001  Peter Johnson
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
#ifndef YASM_PARSER_H
#define YASM_PARSER_H

/* Interface to the parser module(s) -- the "front end" of the assembler */
struct yasm_parser {
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
    yasm_sectionhead *(*do_parse)
	(yasm_preproc *pp, yasm_arch *a, yasm_objfmt *of, yasm_linemgr *lm,
	 yasm_errwarn *we, FILE *f, const char *in_filename, int save_input);
};
#endif
