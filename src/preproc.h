/* $IdPath$
 * YASM preprocessor module interface header file
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
#ifndef YASM_PREPROC_H
#define YASM_PREPROC_H

/* Interface to the preprocesor module(s) */
struct preproc {
    /* one-line description of the preprocessor */
    const char *name;

    /* keyword used to select preprocessor on the command line */
    const char *keyword;

    /* Initializes preprocessor.
     *
     * The preprocessor needs access to the object format module to find out
     * any output format specific macros.
     *
     * This function also takes the FILE * to the initial starting file and
     * the filename.
     */
    void (*initialize) (FILE *f, const char *in_filename, linemgr *lm,
			errwarn *we);

    /* Cleans up any allocated memory. */
    void (*cleanup) (void);

    /* Gets more preprocessed source code (up to max_size bytes) into buf.
     * Note that more than a single line may be returned in buf. */
    size_t (*input) (/*@out@*/ char *buf, size_t max_size);
};

#endif
