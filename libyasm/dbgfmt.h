/* $IdPath$
 * YASM debug format module interface header file
 *
 *  Copyright (C) 2002  Peter Johnson
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
#ifndef YASM_DBGFMT_H
#define YASM_DBGFMT_H

/* Interface to the object format module(s) */
struct yasm_dbgfmt {
    /* one-line description of the format */
    const char *name;

    /* keyword used to select format on the command line */
    const char *keyword;

    /* Initializes debug output.  Must be called before any other debug
     * format functions.  The filenames are provided solely for informational
     * purposes.  May be NULL if not needed by the debug format.
     */
    void (*initialize) (const char *in_filename, const char *obj_filename,
			yasm_objfmt *of);

    /* Cleans up anything allocated by initialize.
     * May be NULL if not needed by the debug format.
     */
    void (*cleanup) (void);

    /* DEBUG directive support. */
    void (*directive) (yasm_valparamhead *valparams, unsigned long lindex);
};

#endif
