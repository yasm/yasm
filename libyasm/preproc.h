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

/** Version number of #yasm_preproc interface.  Any functional change to the
 * #yasm_preproc interface should simultaneously increment this number.  This
 * version should be checked by #yasm_preproc loaders to verify that the
 * expected version (the version defined by its libyasm header files) matches
 * the loaded module version (the version defined by the module's libyasm
 * header files).  Doing this will ensure that the module version's function
 * definitions match the module loader's function definitions.  The version
 * number must never be decreased.
 */
#define YASM_PREPROC_VERSION	2

/* Interface to the preprocesor module(s) */
struct yasm_preproc {
    /** Version (see #YASM_PREPROC_VERSION).  Should always be set to
     * #YASM_PREPROC_VERSION by the module source and checked against
     * #YASM_PREPROC_VERSION by the module loader.
     */
    unsigned int version;

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
    void (*initialize) (FILE *f, const char *in_filename, yasm_linemap *lm);

    /* Cleans up any allocated memory. */
    void (*cleanup) (void);

    /* Gets more preprocessed source code (up to max_size bytes) into buf.
     * Note that more than a single line may be returned in buf. */
    size_t (*input) (/*@out@*/ char *buf, size_t max_size);

    /* Add a directory to the %include search path */
    void (*add_include_path) (const char *path);

    /* Pre-include a file */
    void (*add_include_file) (const char *filename);

    /* Pre-define a macro */
    void (*predefine_macro) (const char *macronameval);

    /* Un-define a macro */
    void (*undefine_macro) (const char *macroname);
};

#endif
